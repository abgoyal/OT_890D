
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/mips-boards/bonito64.h>
#include <asm/mach-lemote/pci.h>

extern struct pci_ops bonito64_pci_ops;

static struct resource loongson2e_pci_mem_resource = {
	.name   = "LOONGSON2E PCI MEM",
	.start  = LOONGSON2E_PCI_MEM_START,
	.end    = LOONGSON2E_PCI_MEM_END,
	.flags  = IORESOURCE_MEM,
};

static struct resource loongson2e_pci_io_resource = {
	.name   = "LOONGSON2E PCI IO MEM",
	.start  = LOONGSON2E_PCI_IO_START,
	.end    = IO_SPACE_LIMIT,
	.flags  = IORESOURCE_IO,
};

static struct pci_controller  loongson2e_pci_controller = {
	.pci_ops        = &bonito64_pci_ops,
	.io_resource    = &loongson2e_pci_io_resource,
	.mem_resource   = &loongson2e_pci_mem_resource,
	.mem_offset     = 0x00000000UL,
	.io_offset      = 0x00000000UL,
};

static void __init ict_pcimap(void)
{
	/*
	 * local to PCI mapping: [256M,512M] -> [256M,512M]; differ from PMON
	 *
	 * CPU address space [256M,448M] is window for accessing pci space
	 * we set pcimap_lo[0,1,2] to map it to pci space [256M,448M]
	 * pcimap: bit18,pcimap_2; bit[17-12],lo2;bit[11-6],lo1;bit[5-0],lo0
	 */
	/* 1,00 0110 ,0001 01,00 0000 */
	BONITO_PCIMAP = 0x46140;

	/* 1, 00 0010, 0000,01, 00 0000 */
	/* BONITO_PCIMAP = 0x42040; */

	/*
	 * PCI to local mapping: [2G,2G+256M] -> [0,256M]
	 */
	BONITO_PCIBASE0 = 0x80000000;
	BONITO_PCIBASE1 = 0x00800000;
	BONITO_PCIBASE2 = 0x90000000;

}

static int __init pcibios_init(void)
{
	ict_pcimap();

	loongson2e_pci_controller.io_map_base =
	    (unsigned long) ioremap(LOONGSON2E_IO_PORT_BASE,
				    loongson2e_pci_io_resource.end -
				    loongson2e_pci_io_resource.start + 1);

	register_pci_controller(&loongson2e_pci_controller);

	return 0;
}

arch_initcall(pcibios_init);
