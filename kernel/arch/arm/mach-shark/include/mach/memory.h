
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#include <asm/sizes.h>

#define PHYS_OFFSET     UL(0x08000000)

#ifndef __ASSEMBLY__

static inline void __arch_adjust_zones(int node, unsigned long *zone_size, unsigned long *zhole_size) 
{
  if (node != 0) return;
  /* Only the first 4 MB (=1024 Pages) are usable for DMA */
  zone_size[1] = zone_size[0] - 1024;
  zone_size[0] = 1024;
  zhole_size[1] = zhole_size[0];
  zhole_size[0] = 0;
}

#define arch_adjust_zones(node, size, holes) \
	__arch_adjust_zones(node, size, holes)

#define ISA_DMA_THRESHOLD	(PHYS_OFFSET + SZ_4M - 1)
#define MAX_DMA_ADDRESS		(PAGE_OFFSET + SZ_4M)

#endif

#define FLUSH_BASE_PHYS		0x80000000
#define FLUSH_BASE		0xdf000000

#endif
