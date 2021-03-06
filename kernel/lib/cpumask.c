
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/bootmem.h>

int __first_cpu(const cpumask_t *srcp)
{
	return min_t(int, NR_CPUS, find_first_bit(srcp->bits, NR_CPUS));
}
EXPORT_SYMBOL(__first_cpu);

int __next_cpu(int n, const cpumask_t *srcp)
{
	return min_t(int, NR_CPUS, find_next_bit(srcp->bits, NR_CPUS, n+1));
}
EXPORT_SYMBOL(__next_cpu);

#if NR_CPUS > 64
int __next_cpu_nr(int n, const cpumask_t *srcp)
{
	return min_t(int, nr_cpu_ids,
				find_next_bit(srcp->bits, nr_cpu_ids, n+1));
}
EXPORT_SYMBOL(__next_cpu_nr);
#endif

int __any_online_cpu(const cpumask_t *mask)
{
	int cpu;

	for_each_cpu_mask(cpu, *mask) {
		if (cpu_online(cpu))
			break;
	}
	return cpu;
}
EXPORT_SYMBOL(__any_online_cpu);

int cpumask_next_and(int n, const struct cpumask *src1p,
		     const struct cpumask *src2p)
{
	while ((n = cpumask_next(n, src1p)) < nr_cpu_ids)
		if (cpumask_test_cpu(n, src2p))
			break;
	return n;
}
EXPORT_SYMBOL(cpumask_next_and);

int cpumask_any_but(const struct cpumask *mask, unsigned int cpu)
{
	unsigned int i;

	cpumask_check(cpu);
	for_each_cpu(i, mask)
		if (i != cpu)
			break;
	return i;
}

/* These are not inline because of header tangles. */
#ifdef CONFIG_CPUMASK_OFFSTACK
bool alloc_cpumask_var_node(cpumask_var_t *mask, gfp_t flags, int node)
{
	if (likely(slab_is_available()))
		*mask = kmalloc_node(cpumask_size(), flags, node);
	else {
#ifdef CONFIG_DEBUG_PER_CPU_MAPS
		printk(KERN_ERR
			"=> alloc_cpumask_var: kmalloc not available!\n");
#endif
		*mask = NULL;
	}
#ifdef CONFIG_DEBUG_PER_CPU_MAPS
	if (!*mask) {
		printk(KERN_ERR "=> alloc_cpumask_var: failed!\n");
		dump_stack();
	}
#endif
	/* FIXME: Bandaid to save us from old primitives which go to NR_CPUS. */
	if (*mask) {
		unsigned int tail;
		tail = BITS_TO_LONGS(NR_CPUS - nr_cpumask_bits) * sizeof(long);
		memset(cpumask_bits(*mask) + cpumask_size() - tail,
		       0, tail);
	}

	return *mask != NULL;
}
EXPORT_SYMBOL(alloc_cpumask_var_node);

bool alloc_cpumask_var(cpumask_var_t *mask, gfp_t flags)
{
	return alloc_cpumask_var_node(mask, flags, numa_node_id());
}
EXPORT_SYMBOL(alloc_cpumask_var);

void __init alloc_bootmem_cpumask_var(cpumask_var_t *mask)
{
	*mask = alloc_bootmem(cpumask_size());
}

void free_cpumask_var(cpumask_var_t mask)
{
	kfree(mask);
}
EXPORT_SYMBOL(free_cpumask_var);

void __init free_bootmem_cpumask_var(cpumask_var_t mask)
{
	free_bootmem((unsigned long)mask, cpumask_size());
}
#endif
