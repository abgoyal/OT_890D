

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of_platform.h>

#include <asm/machdep.h>
#include <asm/ipic.h>
#include <asm/prom.h>
#include <asm/time.h>

#include <sysdev/fsl_pci.h>

#include "mpc512x.h"
#include "mpc5121_ads.h"

static void __init mpc5121_ads_setup_arch(void)
{
#ifdef CONFIG_PCI
	struct device_node *np;
#endif
	printk(KERN_INFO "MPC5121 ADS board from Freescale Semiconductor\n");
	/*
	 * cpld regs are needed early
	 */
	mpc5121_ads_cpld_map();

#ifdef CONFIG_PCI
	for_each_compatible_node(np, "pci", "fsl,mpc5121-pci")
		mpc83xx_add_bridge(np);
#endif
}

static void __init mpc5121_ads_init_IRQ(void)
{
	mpc512x_init_IRQ();
	mpc5121_ads_cpld_pic_init();
}

static int __init mpc5121_ads_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	return of_flat_dt_is_compatible(root, "fsl,mpc5121ads");
}

define_machine(mpc5121_ads) {
	.name			= "MPC5121 ADS",
	.probe			= mpc5121_ads_probe,
	.setup_arch		= mpc5121_ads_setup_arch,
	.init			= mpc512x_declare_of_platform_devices,
	.init_IRQ		= mpc5121_ads_init_IRQ,
	.get_irq		= ipic_get_irq,
	.calibrate_decr		= generic_calibrate_decr,
};
