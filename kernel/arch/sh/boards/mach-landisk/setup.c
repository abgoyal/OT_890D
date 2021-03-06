
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ata_platform.h>
#include <linux/pm.h>
#include <linux/mm.h>
#include <asm/machvec.h>
#include <mach-landisk/mach/iodata_landisk.h>
#include <asm/io.h>

void init_landisk_IRQ(void);

static void landisk_power_off(void)
{
        ctrl_outb(0x01, PA_SHUTDOWN);
}

static struct resource cf_ide_resources[3];

static struct pata_platform_info pata_info = {
	.ioport_shift	= 1,
};

static struct platform_device cf_ide_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(cf_ide_resources),
	.resource	= cf_ide_resources,
	.dev		= {
		.platform_data = &pata_info,
	},
};

static struct platform_device rtc_device = {
	.name		= "rs5c313",
	.id		= -1,
};

static struct platform_device *landisk_devices[] __initdata = {
	&cf_ide_device,
	&rtc_device,
};

static int __init landisk_devices_setup(void)
{
	pgprot_t prot;
	unsigned long paddrbase;
	void *cf_ide_base;

	/* open I/O area window */
	paddrbase = virt_to_phys((void *)PA_AREA5_IO);
	prot = PAGE_KERNEL_PCC(1, _PAGE_PCC_IO16);
	cf_ide_base = p3_ioremap(paddrbase, PAGE_SIZE, prot.pgprot);
	if (!cf_ide_base) {
		printk("allocate_cf_area : can't open CF I/O window!\n");
		return -ENOMEM;
	}

	/* IDE cmd address : 0x1f0-0x1f7 and 0x3f6 */
	cf_ide_resources[0].start = (unsigned long)cf_ide_base + 0x40;
	cf_ide_resources[0].end   = (unsigned long)cf_ide_base + 0x40 + 0x0f;
	cf_ide_resources[0].flags = IORESOURCE_IO;
	cf_ide_resources[1].start = (unsigned long)cf_ide_base + 0x2c;
	cf_ide_resources[1].end   = (unsigned long)cf_ide_base + 0x2c + 0x03;
	cf_ide_resources[1].flags = IORESOURCE_IO;
	cf_ide_resources[2].start = IRQ_FATA;
	cf_ide_resources[2].flags = IORESOURCE_IRQ;

	return platform_add_devices(landisk_devices,
				    ARRAY_SIZE(landisk_devices));
}

__initcall(landisk_devices_setup);

static void __init landisk_setup(char **cmdline_p)
{
        /* LED ON */
	ctrl_outb(ctrl_inb(PA_LED) | 0x03, PA_LED);

	printk(KERN_INFO "I-O DATA DEVICE, INC. \"LANDISK Series\" support.\n");
	pm_power_off = landisk_power_off;
}

static struct sh_machine_vector mv_landisk __initmv = {
	.mv_name = "LANDISK",
	.mv_nr_irqs = 72,
	.mv_setup = landisk_setup,
	.mv_init_irq = init_landisk_IRQ,
};
