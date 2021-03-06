

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include "gru.h"
#include "grutables.h"
#include "grulib.h"
#include "gru_instructions.h"
#include <asm/uv/uv_hub.h>

static inline int is_gru_paddr(unsigned long paddr)
{
	return paddr >= gru_start_paddr && paddr < gru_end_paddr;
}

struct vm_area_struct *gru_find_vma(unsigned long vaddr)
{
	struct vm_area_struct *vma;

	vma = find_vma(current->mm, vaddr);
	if (vma && vma->vm_start <= vaddr && vma->vm_ops == &gru_vm_ops)
		return vma;
	return NULL;
}


static struct gru_thread_state *gru_find_lock_gts(unsigned long vaddr)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	struct gru_thread_state *gts = NULL;

	down_read(&mm->mmap_sem);
	vma = gru_find_vma(vaddr);
	if (vma)
		gts = gru_find_thread_state(vma, TSID(vaddr, vma));
	if (gts)
		mutex_lock(&gts->ts_ctxlock);
	else
		up_read(&mm->mmap_sem);
	return gts;
}

static struct gru_thread_state *gru_alloc_locked_gts(unsigned long vaddr)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	struct gru_thread_state *gts = NULL;

	down_write(&mm->mmap_sem);
	vma = gru_find_vma(vaddr);
	if (vma)
		gts = gru_alloc_thread_state(vma, TSID(vaddr, vma));
	if (gts) {
		mutex_lock(&gts->ts_ctxlock);
		downgrade_write(&mm->mmap_sem);
	} else {
		up_write(&mm->mmap_sem);
	}

	return gts;
}

static void gru_unlock_gts(struct gru_thread_state *gts)
{
	mutex_unlock(&gts->ts_ctxlock);
	up_read(&current->mm->mmap_sem);
}

static void gru_cb_set_istatus_active(unsigned long __user *cb)
{
	union {
		struct gru_instruction_bits bits;
		unsigned long dw;
	} u;

	if (cb) {
		get_user(u.dw, cb);
		u.bits.istatus = CBS_ACTIVE;
		put_user(u.dw, cb);
	}
}

static inline struct gru_state *irq_to_gru(int irq)
{
	return &gru_base[uv_numa_blade_id()]->bs_grus[irq - IRQ_GRU];
}

static void get_clear_fault_map(struct gru_state *gru,
				struct gru_tlb_fault_map *map)
{
	unsigned long i, k;
	struct gru_tlb_fault_map *tfm;

	tfm = get_tfm_for_cpu(gru, gru_cpu_fault_map_id());
	prefetchw(tfm);		/* Helps on hardware, required for emulator */
	for (i = 0; i < BITS_TO_LONGS(GRU_NUM_CBE); i++) {
		k = tfm->fault_bits[i];
		if (k)
			k = xchg(&tfm->fault_bits[i], 0UL);
		map->fault_bits[i] = k;
	}

	/*
	 * Not functionally required but helps performance. (Required
	 * on emulator)
	 */
	gru_flush_cache(tfm);
}

static int non_atomic_pte_lookup(struct vm_area_struct *vma,
				 unsigned long vaddr, int write,
				 unsigned long *paddr, int *pageshift)
{
	struct page *page;

	/* ZZZ Need to handle HUGE pages */
	if (is_vm_hugetlb_page(vma))
		return -EFAULT;
	*pageshift = PAGE_SHIFT;
	if (get_user_pages
	    (current, current->mm, vaddr, 1, write, 0, &page, NULL) <= 0)
		return -EFAULT;
	*paddr = page_to_phys(page);
	put_page(page);
	return 0;
}

static int atomic_pte_lookup(struct vm_area_struct *vma, unsigned long vaddr,
	int write, unsigned long *paddr, int *pageshift)
{
	pgd_t *pgdp;
	pmd_t *pmdp;
	pud_t *pudp;
	pte_t pte;

	pgdp = pgd_offset(vma->vm_mm, vaddr);
	if (unlikely(pgd_none(*pgdp)))
		goto err;

	pudp = pud_offset(pgdp, vaddr);
	if (unlikely(pud_none(*pudp)))
		goto err;

	pmdp = pmd_offset(pudp, vaddr);
	if (unlikely(pmd_none(*pmdp)))
		goto err;
#ifdef CONFIG_X86_64
	if (unlikely(pmd_large(*pmdp)))
		pte = *(pte_t *) pmdp;
	else
#endif
		pte = *pte_offset_kernel(pmdp, vaddr);

	if (unlikely(!pte_present(pte) ||
		     (write && (!pte_write(pte) || !pte_dirty(pte)))))
		return 1;

	*paddr = pte_pfn(pte) << PAGE_SHIFT;
#ifdef CONFIG_HUGETLB_PAGE
	*pageshift = is_vm_hugetlb_page(vma) ? HPAGE_SHIFT : PAGE_SHIFT;
#else
	*pageshift = PAGE_SHIFT;
#endif
	return 0;

err:
	local_irq_enable();
	return 1;
}

static int gru_try_dropin(struct gru_thread_state *gts,
			  struct gru_tlb_fault_handle *tfh,
			  unsigned long __user *cb)
{
	struct mm_struct *mm = gts->ts_mm;
	struct vm_area_struct *vma;
	int pageshift, asid, write, ret;
	unsigned long paddr, gpa, vaddr;

	/*
	 * NOTE: The GRU contains magic hardware that eliminates races between
	 * TLB invalidates and TLB dropins. If an invalidate occurs
	 * in the window between reading the TFH and the subsequent TLB dropin,
	 * the dropin is ignored. This eliminates the need for additional locks.
	 */

	/*
	 * Error if TFH state is IDLE or FMM mode & the user issuing a UPM call.
	 * Might be a hardware race OR a stupid user. Ignore FMM because FMM
	 * is a transient state.
	 */
	if (tfh->state == TFHSTATE_IDLE)
		goto failidle;
	if (tfh->state == TFHSTATE_MISS_FMM && cb)
		goto failfmm;

	write = (tfh->cause & TFHCAUSE_TLB_MOD) != 0;
	vaddr = tfh->missvaddr;
	asid = tfh->missasid;
	if (asid == 0)
		goto failnoasid;

	rmb();	/* TFH must be cache resident before reading ms_range_active */

	/*
	 * TFH is cache resident - at least briefly. Fail the dropin
	 * if a range invalidate is active.
	 */
	if (atomic_read(&gts->ts_gms->ms_range_active))
		goto failactive;

	vma = find_vma(mm, vaddr);
	if (!vma)
		goto failinval;

	/*
	 * Atomic lookup is faster & usually works even if called in non-atomic
	 * context.
	 */
	rmb();	/* Must/check ms_range_active before loading PTEs */
	ret = atomic_pte_lookup(vma, vaddr, write, &paddr, &pageshift);
	if (ret) {
		if (!cb)
			goto failupm;
		if (non_atomic_pte_lookup(vma, vaddr, write, &paddr,
					  &pageshift))
			goto failinval;
	}
	if (is_gru_paddr(paddr))
		goto failinval;

	paddr = paddr & ~((1UL << pageshift) - 1);
	gpa = uv_soc_phys_ram_to_gpa(paddr);
	gru_cb_set_istatus_active(cb);
	tfh_write_restart(tfh, gpa, GAA_RAM, vaddr, asid, write,
			  GRU_PAGESIZE(pageshift));
	STAT(tlb_dropin);
	gru_dbg(grudev,
		"%s: tfh 0x%p, vaddr 0x%lx, asid 0x%x, ps %d, gpa 0x%lx\n",
		ret ? "non-atomic" : "atomic", tfh, vaddr, asid,
		pageshift, gpa);
	return 0;

failnoasid:
	/* No asid (delayed unload). */
	STAT(tlb_dropin_fail_no_asid);
	gru_dbg(grudev, "FAILED no_asid tfh: 0x%p, vaddr 0x%lx\n", tfh, vaddr);
	if (!cb)
		tfh_user_polling_mode(tfh);
	else
		gru_flush_cache(tfh);
	return -EAGAIN;

failupm:
	/* Atomic failure switch CBR to UPM */
	tfh_user_polling_mode(tfh);
	STAT(tlb_dropin_fail_upm);
	gru_dbg(grudev, "FAILED upm tfh: 0x%p, vaddr 0x%lx\n", tfh, vaddr);
	return 1;

failfmm:
	/* FMM state on UPM call */
	STAT(tlb_dropin_fail_fmm);
	gru_dbg(grudev, "FAILED fmm tfh: 0x%p, state %d\n", tfh, tfh->state);
	return 0;

failidle:
	/* TFH was idle  - no miss pending */
	gru_flush_cache(tfh);
	if (cb)
		gru_flush_cache(cb);
	STAT(tlb_dropin_fail_idle);
	gru_dbg(grudev, "FAILED idle tfh: 0x%p, state %d\n", tfh, tfh->state);
	return 0;

failinval:
	/* All errors (atomic & non-atomic) switch CBR to EXCEPTION state */
	tfh_exception(tfh);
	STAT(tlb_dropin_fail_invalid);
	gru_dbg(grudev, "FAILED inval tfh: 0x%p, vaddr 0x%lx\n", tfh, vaddr);
	return -EFAULT;

failactive:
	/* Range invalidate active. Switch to UPM iff atomic */
	if (!cb)
		tfh_user_polling_mode(tfh);
	else
		gru_flush_cache(tfh);
	STAT(tlb_dropin_fail_range_active);
	gru_dbg(grudev, "FAILED range active: tfh 0x%p, vaddr 0x%lx\n",
		tfh, vaddr);
	return 1;
}

irqreturn_t gru_intr(int irq, void *dev_id)
{
	struct gru_state *gru;
	struct gru_tlb_fault_map map;
	struct gru_thread_state *gts;
	struct gru_tlb_fault_handle *tfh = NULL;
	int cbrnum, ctxnum;

	STAT(intr);

	gru = irq_to_gru(irq);
	if (!gru) {
		dev_err(grudev, "GRU: invalid interrupt: cpu %d, irq %d\n",
			raw_smp_processor_id(), irq);
		return IRQ_NONE;
	}
	get_clear_fault_map(gru, &map);
	gru_dbg(grudev, "irq %d, gru %x, map 0x%lx\n", irq, gru->gs_gid,
		map.fault_bits[0]);

	for_each_cbr_in_tfm(cbrnum, map.fault_bits) {
		tfh = get_tfh_by_index(gru, cbrnum);
		prefetchw(tfh);	/* Helps on hdw, required for emulator */

		/*
		 * When hardware sets a bit in the faultmap, it implicitly
		 * locks the GRU context so that it cannot be unloaded.
		 * The gts cannot change until a TFH start/writestart command
		 * is issued.
		 */
		ctxnum = tfh->ctxnum;
		gts = gru->gs_gts[ctxnum];

		/*
		 * This is running in interrupt context. Trylock the mmap_sem.
		 * If it fails, retry the fault in user context.
		 */
		if (down_read_trylock(&gts->ts_mm->mmap_sem)) {
			gru_try_dropin(gts, tfh, NULL);
			up_read(&gts->ts_mm->mmap_sem);
		} else {
			tfh_user_polling_mode(tfh);
		}
	}
	return IRQ_HANDLED;
}


static int gru_user_dropin(struct gru_thread_state *gts,
			   struct gru_tlb_fault_handle *tfh,
			   unsigned long __user *cb)
{
	struct gru_mm_struct *gms = gts->ts_gms;
	int ret;

	while (1) {
		wait_event(gms->ms_wait_queue,
			   atomic_read(&gms->ms_range_active) == 0);
		prefetchw(tfh);	/* Helps on hdw, required for emulator */
		ret = gru_try_dropin(gts, tfh, cb);
		if (ret <= 0)
			return ret;
		STAT(call_os_wait_queue);
	}
}

int gru_handle_user_call_os(unsigned long cb)
{
	struct gru_tlb_fault_handle *tfh;
	struct gru_thread_state *gts;
	unsigned long __user *cbp;
	int ucbnum, cbrnum, ret = -EINVAL;

	STAT(call_os);
	gru_dbg(grudev, "address 0x%lx\n", cb);

	/* sanity check the cb pointer */
	ucbnum = get_cb_number((void *)cb);
	if ((cb & (GRU_HANDLE_STRIDE - 1)) || ucbnum >= GRU_NUM_CB)
		return -EINVAL;
	cbp = (unsigned long *)cb;

	gts = gru_find_lock_gts(cb);
	if (!gts)
		return -EINVAL;

	if (ucbnum >= gts->ts_cbr_au_count * GRU_CBR_AU_SIZE) {
		ret = -EINVAL;
		goto exit;
	}

	/*
	 * If force_unload is set, the UPM TLB fault is phony. The task
	 * has migrated to another node and the GSEG must be moved. Just
	 * unload the context. The task will page fault and assign a new
	 * context.
	 */
	ret = -EAGAIN;
	cbrnum = thread_cbr_number(gts, ucbnum);
	if (gts->ts_force_unload) {
		gru_unload_context(gts, 1);
	} else if (gts->ts_gru) {
		tfh = get_tfh_by_index(gts->ts_gru, cbrnum);
		ret = gru_user_dropin(gts, tfh, cbp);
	}
exit:
	gru_unlock_gts(gts);
	return ret;
}

int gru_get_exception_detail(unsigned long arg)
{
	struct control_block_extended_exc_detail excdet;
	struct gru_control_block_extended *cbe;
	struct gru_thread_state *gts;
	int ucbnum, cbrnum, ret;

	STAT(user_exception);
	if (copy_from_user(&excdet, (void __user *)arg, sizeof(excdet)))
		return -EFAULT;

	gru_dbg(grudev, "address 0x%lx\n", excdet.cb);
	gts = gru_find_lock_gts(excdet.cb);
	if (!gts)
		return -EINVAL;

	if (gts->ts_gru) {
		ucbnum = get_cb_number((void *)excdet.cb);
		cbrnum = thread_cbr_number(gts, ucbnum);
		cbe = get_cbe_by_index(gts->ts_gru, cbrnum);
		prefetchw(cbe);		/* Harmless on hardware, required for emulator */
		excdet.opc = cbe->opccpy;
		excdet.exopc = cbe->exopccpy;
		excdet.ecause = cbe->ecause;
		excdet.exceptdet0 = cbe->idef1upd;
		excdet.exceptdet1 = cbe->idef3upd;
		ret = 0;
	} else {
		ret = -EAGAIN;
	}
	gru_unlock_gts(gts);

	gru_dbg(grudev, "address 0x%lx, ecause 0x%x\n", excdet.cb,
		excdet.ecause);
	if (!ret && copy_to_user((void __user *)arg, &excdet, sizeof(excdet)))
		ret = -EFAULT;
	return ret;
}

int gru_user_unload_context(unsigned long arg)
{
	struct gru_thread_state *gts;
	struct gru_unload_context_req req;

	STAT(user_unload_context);
	if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
		return -EFAULT;

	gru_dbg(grudev, "gseg 0x%lx\n", req.gseg);

	gts = gru_find_lock_gts(req.gseg);
	if (!gts)
		return -EINVAL;

	if (gts->ts_gru)
		gru_unload_context(gts, 1);
	gru_unlock_gts(gts);

	return 0;
}

int gru_user_flush_tlb(unsigned long arg)
{
	struct gru_thread_state *gts;
	struct gru_flush_tlb_req req;

	STAT(user_flush_tlb);
	if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
		return -EFAULT;

	gru_dbg(grudev, "gseg 0x%lx, vaddr 0x%lx, len 0x%lx\n", req.gseg,
		req.vaddr, req.len);

	gts = gru_find_lock_gts(req.gseg);
	if (!gts)
		return -EINVAL;

	gru_flush_tlb_range(gts->ts_gms, req.vaddr, req.vaddr + req.len);
	gru_unlock_gts(gts);

	return 0;
}

int gru_set_task_slice(long address)
{
	struct gru_thread_state *gts;

	STAT(set_task_slice);
	gru_dbg(grudev, "address 0x%lx\n", address);
	gts = gru_alloc_locked_gts(address);
	if (!gts)
		return -EINVAL;

	gts->ts_tgid_owner = current->tgid;
	gru_unlock_gts(gts);

	return 0;
}
