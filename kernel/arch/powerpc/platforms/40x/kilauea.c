
#include <linux/init.h>
#include <linux/of_platform.h>
#include <asm/machdep.h>
#include <asm/prom.h>
#include <asm/udbg.h>
#include <asm/time.h>
#include <asm/uic.h>
#include <asm/pci-bridge.h>
#include <asm/ppc4xx.h>

static __initdata struct of_device_id kilauea_of_bus[] = {
	{ .compatible = "ibm,plb4", },
	{ .compatible = "ibm,opb", },
	{ .compatible = "ibm,ebc", },
	{},
};

static int __init kilauea_device_probe(void)
{
	of_platform_bus_probe(NULL, kilauea_of_bus, NULL);

	return 0;
}
machine_device_initcall(kilauea, kilauea_device_probe);

static int __init kilauea_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	if (!of_flat_dt_is_compatible(root, "amcc,kilauea"))
		return 0;

	ppc_pci_set_flags(PPC_PCI_REASSIGN_ALL_RSRC);

	return 1;
}

define_machine(kilauea) {
	.name 				= "Kilauea",
	.probe 				= kilauea_probe,
	.progress 			= udbg_progress,
	.init_IRQ 			= uic_init_tree,
	.get_irq 			= uic_get_irq,
	.restart			= ppc4xx_reset_system,
	.calibrate_decr			= generic_calibrate_decr,
};
