
#ifndef __ARCH_BLACKFIN_CACHE_H
#define __ARCH_BLACKFIN_CACHE_H

#define L1_CACHE_SHIFT	5
#define L1_CACHE_BYTES	(1 << L1_CACHE_SHIFT)
#define SMP_CACHE_BYTES	L1_CACHE_BYTES

#ifdef CONFIG_SMP
#define __cacheline_aligned
#else
#define ____cacheline_aligned

#ifdef CONFIG_CACHELINE_ALIGNED_L1
#define __cacheline_aligned				\
	  __attribute__((__aligned__(L1_CACHE_BYTES),	\
		__section__(".data_l1.cacheline_aligned")))
#endif

#endif

#define L1_CACHE_SHIFT_MAX	5

#if defined(CONFIG_SMP) && \
    !defined(CONFIG_BFIN_CACHE_COHERENT) && \
    defined(CONFIG_BFIN_DCACHE)
#define __ARCH_SYNC_CORE_DCACHE
#ifndef __ASSEMBLY__
asmlinkage void __raw_smp_mark_barrier_asm(void);
asmlinkage void __raw_smp_check_barrier_asm(void);

static inline void smp_mark_barrier(void)
{
	__raw_smp_mark_barrier_asm();
}
static inline void smp_check_barrier(void)
{
	__raw_smp_check_barrier_asm();
}

void resync_core_dcache(void);
#endif
#endif


#endif
