

#ifndef _ASM_X86_MMZONE_32_H
#define _ASM_X86_MMZONE_32_H

#include <asm/smp.h>

#ifdef CONFIG_NUMA
extern struct pglist_data *node_data[];
#define NODE_DATA(nid)	(node_data[nid])

#include <asm/numaq.h>
/* summit or generic arch */
#include <asm/srat.h>

extern int get_memcfg_numa_flat(void);
static inline void get_memcfg_numa(void)
{

	if (get_memcfg_numaq())
		return;
	if (get_memcfg_from_srat())
		return;
	get_memcfg_numa_flat();
}

extern void resume_map_numa_kva(pgd_t *pgd);

#else /* !CONFIG_NUMA */

#define get_memcfg_numa get_memcfg_numa_flat

static inline void resume_map_numa_kva(pgd_t *pgd) {}

#endif /* CONFIG_NUMA */

#ifdef CONFIG_DISCONTIGMEM

#define MAX_NR_PAGES 16777216
#define MAX_ELEMENTS 1024
#define PAGES_PER_ELEMENT (MAX_NR_PAGES/MAX_ELEMENTS)

extern s8 physnode_map[];

static inline int pfn_to_nid(unsigned long pfn)
{
#ifdef CONFIG_NUMA
	return((int) physnode_map[(pfn) / PAGES_PER_ELEMENT]);
#else
	return 0;
#endif
}


#define node_start_pfn(nid)	(NODE_DATA(nid)->node_start_pfn)
#define node_end_pfn(nid)						\
({									\
	pg_data_t *__pgdat = NODE_DATA(nid);				\
	__pgdat->node_start_pfn + __pgdat->node_spanned_pages;		\
})

static inline int pfn_valid(int pfn)
{
	int nid = pfn_to_nid(pfn);

	if (nid >= 0)
		return (pfn < node_end_pfn(nid));
	return 0;
}

#endif /* CONFIG_DISCONTIGMEM */

#ifdef CONFIG_NEED_MULTIPLE_NODES

#define reserve_bootmem(addr, size, flags) \
	reserve_bootmem_node(NODE_DATA(0), (addr), (size), (flags))
#define alloc_bootmem(x) \
	__alloc_bootmem_node(NODE_DATA(0), (x), SMP_CACHE_BYTES, __pa(MAX_DMA_ADDRESS))
#define alloc_bootmem_nopanic(x) \
	__alloc_bootmem_node_nopanic(NODE_DATA(0), (x), SMP_CACHE_BYTES, \
				__pa(MAX_DMA_ADDRESS))
#define alloc_bootmem_low(x) \
	__alloc_bootmem_node(NODE_DATA(0), (x), SMP_CACHE_BYTES, 0)
#define alloc_bootmem_pages(x) \
	__alloc_bootmem_node(NODE_DATA(0), (x), PAGE_SIZE, __pa(MAX_DMA_ADDRESS))
#define alloc_bootmem_pages_nopanic(x) \
	__alloc_bootmem_node_nopanic(NODE_DATA(0), (x), PAGE_SIZE, \
				__pa(MAX_DMA_ADDRESS))
#define alloc_bootmem_low_pages(x) \
	__alloc_bootmem_node(NODE_DATA(0), (x), PAGE_SIZE, 0)
#define alloc_bootmem_node(pgdat, x)					\
({									\
	struct pglist_data  __maybe_unused			\
				*__alloc_bootmem_node__pgdat = (pgdat);	\
	__alloc_bootmem_node(NODE_DATA(0), (x), SMP_CACHE_BYTES,	\
						__pa(MAX_DMA_ADDRESS));	\
})
#define alloc_bootmem_pages_node(pgdat, x)				\
({									\
	struct pglist_data  __maybe_unused			\
				*__alloc_bootmem_node__pgdat = (pgdat);	\
	__alloc_bootmem_node(NODE_DATA(0), (x), PAGE_SIZE,		\
						__pa(MAX_DMA_ADDRESS));	\
})
#define alloc_bootmem_low_pages_node(pgdat, x)				\
({									\
	struct pglist_data  __maybe_unused			\
				*__alloc_bootmem_node__pgdat = (pgdat);	\
	__alloc_bootmem_node(NODE_DATA(0), (x), PAGE_SIZE, 0);		\
})
#endif /* CONFIG_NEED_MULTIPLE_NODES */

#endif /* _ASM_X86_MMZONE_32_H */
