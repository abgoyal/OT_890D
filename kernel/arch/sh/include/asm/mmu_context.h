
#ifndef __ASM_SH_MMU_CONTEXT_H
#define __ASM_SH_MMU_CONTEXT_H

#ifdef __KERNEL__
#include <cpu/mmu_context.h>
#include <asm/tlbflush.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm-generic/mm_hooks.h>

#define MMU_CONTEXT_ASID_MASK		0x000000ff
#define MMU_CONTEXT_VERSION_MASK	0xffffff00
#define MMU_CONTEXT_FIRST_VERSION	0x00000100
#define NO_CONTEXT			0UL

/* ASID is 8-bit value, so it can't be 0x100 */
#define MMU_NO_ASID			0x100

#define asid_cache(cpu)		(cpu_data[cpu].asid_cache)

#ifdef CONFIG_MMU
#define cpu_context(cpu, mm)	((mm)->context.id[cpu])

#define cpu_asid(cpu, mm)	\
	(cpu_context((cpu), (mm)) & MMU_CONTEXT_ASID_MASK)

#define MMU_VPN_MASK	0xfffff000

#if defined(CONFIG_SUPERH32)
#include "mmu_context_32.h"
#else
#include "mmu_context_64.h"
#endif

static inline void get_mmu_context(struct mm_struct *mm, unsigned int cpu)
{
	unsigned long asid = asid_cache(cpu);

	/* Check if we have old version of context. */
	if (((cpu_context(cpu, mm) ^ asid) & MMU_CONTEXT_VERSION_MASK) == 0)
		/* It's up to date, do nothing */
		return;

	/* It's old, we need to get new context with new version. */
	if (!(++asid & MMU_CONTEXT_ASID_MASK)) {
		/*
		 * We exhaust ASID of this version.
		 * Flush all TLB and start new cycle.
		 */
		flush_tlb_all();

#ifdef CONFIG_SUPERH64
		/*
		 * The SH-5 cache uses the ASIDs, requiring both the I and D
		 * cache to be flushed when the ASID is exhausted. Weak.
		 */
		flush_cache_all();
#endif

		/*
		 * Fix version; Note that we avoid version #0
		 * to distingush NO_CONTEXT.
		 */
		if (!asid)
			asid = MMU_CONTEXT_FIRST_VERSION;
	}

	cpu_context(cpu, mm) = asid_cache(cpu) = asid;
}

static inline int init_new_context(struct task_struct *tsk,
				   struct mm_struct *mm)
{
	int i;

	for (i = 0; i < num_online_cpus(); i++)
		cpu_context(i, mm) = NO_CONTEXT;

	return 0;
}

static inline void activate_context(struct mm_struct *mm, unsigned int cpu)
{
	get_mmu_context(mm, cpu);
	set_asid(cpu_asid(cpu, mm));
}

static inline void switch_mm(struct mm_struct *prev,
			     struct mm_struct *next,
			     struct task_struct *tsk)
{
	unsigned int cpu = smp_processor_id();

	if (likely(prev != next)) {
		cpu_set(cpu, next->cpu_vm_mask);
		set_TTB(next->pgd);
		activate_context(next, cpu);
	} else
		if (!cpu_test_and_set(cpu, next->cpu_vm_mask))
			activate_context(next, cpu);
}
#else
#define get_mmu_context(mm)		do { } while (0)
#define init_new_context(tsk,mm)	(0)
#define destroy_context(mm)		do { } while (0)
#define set_asid(asid)			do { } while (0)
#define get_asid()			(0)
#define cpu_asid(cpu, mm)		({ (void)cpu; NO_CONTEXT; })
#define switch_and_save_asid(asid)	(0)
#define set_TTB(pgd)			do { } while (0)
#define get_TTB()			(0)
#define activate_context(mm,cpu)	do { } while (0)
#define switch_mm(prev,next,tsk)	do { } while (0)
#endif /* CONFIG_MMU */

#define activate_mm(prev, next)		switch_mm((prev),(next),NULL)
#define deactivate_mm(tsk,mm)		do { } while (0)
#define enter_lazy_tlb(mm,tsk)		do { } while (0)

#if defined(CONFIG_CPU_SH3) || defined(CONFIG_CPU_SH4)
static inline void enable_mmu(void)
{
	unsigned int cpu = smp_processor_id();

	/* Enable MMU */
	ctrl_outl(MMU_CONTROL_INIT, MMUCR);
	ctrl_barrier();

	if (asid_cache(cpu) == NO_CONTEXT)
		asid_cache(cpu) = MMU_CONTEXT_FIRST_VERSION;

	set_asid(asid_cache(cpu) & MMU_CONTEXT_ASID_MASK);
}

static inline void disable_mmu(void)
{
	unsigned long cr;

	cr = ctrl_inl(MMUCR);
	cr &= ~MMU_CONTROL_INIT;
	ctrl_outl(cr, MMUCR);

	ctrl_barrier();
}
#else
#define enable_mmu()	do { } while (0)
#define disable_mmu()	do { } while (0)
#endif

#endif /* __KERNEL__ */
#endif /* __ASM_SH_MMU_CONTEXT_H */
