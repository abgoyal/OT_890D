
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/serial_sci.h>

static struct plat_sci_port sci_platform_data[] = {
	{
		.mapbase	= 0xffe80000,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCIF,
		.irqs		= { 40, 41, 43, 42 },
	}, {
		.flags = 0,
	}
};

static struct platform_device sci_device = {
	.name		= "sh-sci",
	.id		= -1,
	.dev		= {
		.platform_data	= sci_platform_data,
	},
};

static struct platform_device *sh4202_devices[] __initdata = {
	&sci_device,
};

static int __init sh4202_devices_setup(void)
{
	return platform_add_devices(sh4202_devices,
				    ARRAY_SIZE(sh4202_devices));
}
__initcall(sh4202_devices_setup);

void __init plat_irq_setup(void)
{
	/* do nothing - all IRL interrupts are handled by the board code */
}
