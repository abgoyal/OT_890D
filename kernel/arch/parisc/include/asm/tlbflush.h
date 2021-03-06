
#ifndef _PARISC_TLBFLUSH_H
#define _PARISC_TLBFLUSH_H

/* TLB flushing routines.... */

#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/mmu_context.h>


extern spinlock_t pa_tlb_lock;

#define purge_tlb_start(x) spin_lock(&pa_tlb_lock)
#define purge_tlb_end(x) spin_unlock(&pa_tlb_lock)

extern void flush_tlb_all(void);
extern void flush_tlb_all_local(void *);


static inline void flush_tlb_mm(struct mm_struct *mm)
{
	BUG_ON(mm == &init_mm); /* Should never happen */

#if 1 || defined(CONFIG_SMP)
	flush_tlb_all();
#else
	/* FIXME: currently broken, causing space id and protection ids
	 *  to go out of sync, resulting in faults on userspace accesses.
	 */
	if (mm) {
		if (mm->context != 0)
			free_sid(mm->context);
		mm->context = alloc_sid();
		if (mm == current->active_mm)
			load_context(mm->context);
	}
#endif
}

static inline void flush_tlb_page(struct vm_area_struct *vma,
	unsigned long addr)
{
	/* For one page, it's not worth testing the split_tlb variable */

	mb();
	mtsp(vma->vm_mm->context,1);
	purge_tlb_start();
	pdtlb(addr);
	pitlb(addr);
	purge_tlb_end();
}

void __flush_tlb_range(unsigned long sid,
	unsigned long start, unsigned long end);

#define flush_tlb_range(vma,start,end) __flush_tlb_range((vma)->vm_mm->context,start,end)

#define flush_tlb_kernel_range(start, end) __flush_tlb_range(0,start,end)

#endif
