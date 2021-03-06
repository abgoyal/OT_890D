
#ifndef _ASM_POWERPC_CPUTHREADS_H
#define _ASM_POWERPC_CPUTHREADS_H

#include <linux/cpumask.h>


#ifdef CONFIG_SMP
extern int threads_per_core;
extern int threads_shift;
extern cpumask_t threads_core_mask;
#else
#define threads_per_core	1
#define threads_shift		0
#define threads_core_mask	(CPU_MASK_CPU0)
#endif

static inline cpumask_t cpu_thread_mask_to_cores(cpumask_t threads)
{
	cpumask_t	tmp, res;
	int		i;

	res = CPU_MASK_NONE;
	for (i = 0; i < NR_CPUS; i += threads_per_core) {
		cpus_shift_left(tmp, threads_core_mask, i);
		if (cpus_intersects(threads, tmp))
			cpu_set(i, res);
	}
	return res;
}

static inline int cpu_nr_cores(void)
{
	return NR_CPUS >> threads_shift;
}

static inline cpumask_t cpu_online_cores_map(void)
{
	return cpu_thread_mask_to_cores(cpu_online_map);
}

static inline int cpu_thread_to_core(int cpu)
{
	return cpu >> threads_shift;
}

static inline int cpu_thread_in_core(int cpu)
{
	return cpu & (threads_per_core - 1);
}

static inline int cpu_first_thread_in_core(int cpu)
{
	return cpu & ~(threads_per_core - 1);
}

#endif /* _ASM_POWERPC_CPUTHREADS_H */

