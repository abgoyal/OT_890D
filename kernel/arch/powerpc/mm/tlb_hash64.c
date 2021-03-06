

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/percpu.h>
#include <linux/hardirq.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include <asm/tlb.h>
#include <asm/bug.h>

DEFINE_PER_CPU(struct ppc64_tlb_batch, ppc64_tlb_batch);

DEFINE_PER_CPU(struct mmu_gather, mmu_gathers);

void hpte_need_flush(struct mm_struct *mm, unsigned long addr,
		     pte_t *ptep, unsigned long pte, int huge)
{
	struct ppc64_tlb_batch *batch = &__get_cpu_var(ppc64_tlb_batch);
	unsigned long vsid, vaddr;
	unsigned int psize;
	int ssize;
	real_pte_t rpte;
	int i;

	i = batch->index;

	/* We mask the address for the base page size. Huge pages will
	 * have applied their own masking already
	 */
	addr &= PAGE_MASK;

	/* Get page size (maybe move back to caller).
	 *
	 * NOTE: when using special 64K mappings in 4K environment like
	 * for SPEs, we obtain the page size from the slice, which thus
	 * must still exist (and thus the VMA not reused) at the time
	 * of this call
	 */
	if (huge) {
#ifdef CONFIG_HUGETLB_PAGE
		psize = get_slice_psize(mm, addr);;
#else
		BUG();
		psize = pte_pagesize_index(mm, addr, pte); /* shutup gcc */
#endif
	} else
		psize = pte_pagesize_index(mm, addr, pte);

	/* Build full vaddr */
	if (!is_kernel_addr(addr)) {
		ssize = user_segment_size(addr);
		vsid = get_vsid(mm->context.id, addr, ssize);
		WARN_ON(vsid == 0);
	} else {
		vsid = get_kernel_vsid(addr, mmu_kernel_ssize);
		ssize = mmu_kernel_ssize;
	}
	vaddr = hpt_va(addr, vsid, ssize);
	rpte = __real_pte(__pte(pte), ptep);

	/*
	 * Check if we have an active batch on this CPU. If not, just
	 * flush now and return. For now, we don global invalidates
	 * in that case, might be worth testing the mm cpu mask though
	 * and decide to use local invalidates instead...
	 */
	if (!batch->active) {
		flush_hash_page(vaddr, rpte, psize, ssize, 0);
		return;
	}

	/*
	 * This can happen when we are in the middle of a TLB batch and
	 * we encounter memory pressure (eg copy_page_range when it tries
	 * to allocate a new pte). If we have to reclaim memory and end
	 * up scanning and resetting referenced bits then our batch context
	 * will change mid stream.
	 *
	 * We also need to ensure only one page size is present in a given
	 * batch
	 */
	if (i != 0 && (mm != batch->mm || batch->psize != psize ||
		       batch->ssize != ssize)) {
		__flush_tlb_pending(batch);
		i = 0;
	}
	if (i == 0) {
		batch->mm = mm;
		batch->psize = psize;
		batch->ssize = ssize;
	}
	batch->pte[i] = rpte;
	batch->vaddr[i] = vaddr;
	batch->index = ++i;
	if (i >= PPC64_TLB_BATCH_NR)
		__flush_tlb_pending(batch);
}

void __flush_tlb_pending(struct ppc64_tlb_batch *batch)
{
	cpumask_t tmp;
	int i, local = 0;

	i = batch->index;
	tmp = cpumask_of_cpu(smp_processor_id());
	if (cpus_equal(batch->mm->cpu_vm_mask, tmp))
		local = 1;
	if (i == 1)
		flush_hash_page(batch->vaddr[0], batch->pte[0],
				batch->psize, batch->ssize, local);
	else
		flush_hash_range(i, local);
	batch->index = 0;
}

#ifdef CONFIG_HOTPLUG

void __flush_hash_table_range(struct mm_struct *mm, unsigned long start,
			      unsigned long end)
{
	unsigned long flags;

	start = _ALIGN_DOWN(start, PAGE_SIZE);
	end = _ALIGN_UP(end, PAGE_SIZE);

	BUG_ON(!mm->pgd);

	/* Note: Normally, we should only ever use a batch within a
	 * PTE locked section. This violates the rule, but will work
	 * since we don't actually modify the PTEs, we just flush the
	 * hash while leaving the PTEs intact (including their reference
	 * to being hashed). This is not the most performance oriented
	 * way to do things but is fine for our needs here.
	 */
	local_irq_save(flags);
	arch_enter_lazy_mmu_mode();
	for (; start < end; start += PAGE_SIZE) {
		pte_t *ptep = find_linux_pte(mm->pgd, start);
		unsigned long pte;

		if (ptep == NULL)
			continue;
		pte = pte_val(*ptep);
		if (!(pte & _PAGE_HASHPTE))
			continue;
		hpte_need_flush(mm, start, ptep, pte, 0);
	}
	arch_leave_lazy_mmu_mode();
	local_irq_restore(flags);
}

#endif /* CONFIG_HOTPLUG */
