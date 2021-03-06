
#ifndef __ASM_MACH_IP32_DMA_COHERENCE_H
#define __ASM_MACH_IP32_DMA_COHERENCE_H

#include <asm/ip32/crime.h>

struct device;


#define RAM_OFFSET_MASK 0x3fffffffUL

static inline dma_addr_t plat_map_dma_mem(struct device *dev, void *addr,
	size_t size)
{
	dma_addr_t pa = virt_to_phys(addr) & RAM_OFFSET_MASK;

	if (dev == NULL)
		pa += CRIME_HI_MEM_BASE;

	return pa;
}

static dma_addr_t plat_map_dma_mem_page(struct device *dev, struct page *page)
{
	dma_addr_t pa;

	pa = page_to_phys(page) & RAM_OFFSET_MASK;

	if (dev == NULL)
		pa += CRIME_HI_MEM_BASE;

	return pa;
}

/* This is almost certainly wrong but it's what dma-ip32.c used to use  */
static unsigned long plat_dma_addr_to_phys(dma_addr_t dma_addr)
{
	unsigned long addr = dma_addr & RAM_OFFSET_MASK;

	if (dma_addr >= 256*1024*1024)
		addr += CRIME_HI_MEM_BASE;

	return addr;
}

static inline void plat_unmap_dma_mem(struct device *dev, dma_addr_t dma_addr)
{
}

static inline int plat_dma_supported(struct device *dev, u64 mask)
{
	/*
	 * we fall back to GFP_DMA when the mask isn't all 1s,
	 * so we can't guarantee allocations that must be
	 * within a tighter range than GFP_DMA..
	 */
	if (mask < DMA_BIT_MASK(24))
		return 0;

	return 1;
}

static inline void plat_extra_sync_for_device(struct device *dev)
{
	return;
}

static inline int plat_dma_mapping_error(struct device *dev,
					 dma_addr_t dma_addr)
{
	return 0;
}

static inline int plat_device_is_coherent(struct device *dev)
{
	return 0;		/* IP32 is non-cohernet */
}

#endif /* __ASM_MACH_IP32_DMA_COHERENCE_H */
