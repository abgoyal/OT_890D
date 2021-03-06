
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define PHYS_OFFSET	UL(0x00000000)

#define BUS_OFFSET	UL(0x80000000)
#define __virt_to_bus(x)	((x) - PAGE_OFFSET + BUS_OFFSET)
#define __bus_to_virt(x)	((x) - BUS_OFFSET + PAGE_OFFSET)

#endif
