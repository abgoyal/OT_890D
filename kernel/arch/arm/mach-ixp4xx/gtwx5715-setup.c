

#include <linux/init.h>
#include <linux/device.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/serial_8250.h>
#include <linux/slab.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <mach/gtwx5715.h>


#ifdef	__ARMEB__
#define	REG_OFFSET	3
#else
#define	REG_OFFSET	0
#endif


static struct resource gtwx5715_uart_resources[] = {
	{
		.start	= IXP4XX_UART2_BASE_PHYS,
		.end	= IXP4XX_UART2_BASE_PHYS + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= IRQ_IXP4XX_UART2,
		.end	= IRQ_IXP4XX_UART2,
		.flags	= IORESOURCE_IRQ,
	},
	{ },
};


static struct plat_serial8250_port gtwx5715_uart_platform_data[] = {
	{
	.mapbase	= IXP4XX_UART2_BASE_PHYS,
	.membase	= (char *)IXP4XX_UART2_BASE_VIRT + REG_OFFSET,
	.irq		= IRQ_IXP4XX_UART2,
	.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	.iotype		= UPIO_MEM,
	.regshift	= 2,
	.uartclk	= IXP4XX_UART_XTAL,
	},
	{ },
};

static struct platform_device gtwx5715_uart_device = {
	.name		= "serial8250",
	.id		= PLAT8250_DEV_PLATFORM,
	.dev			= {
		.platform_data	= gtwx5715_uart_platform_data,
	},
	.num_resources	= 2,
	.resource	= gtwx5715_uart_resources,
};

static struct flash_platform_data gtwx5715_flash_data = {
	.map_name	= "cfi_probe",
	.width		= 2,
};

static struct resource gtwx5715_flash_resource = {
	.flags		= IORESOURCE_MEM,
};

static struct platform_device gtwx5715_flash = {
	.name		= "IXP4XX-Flash",
	.id		= 0,
	.dev		= {
		.platform_data = &gtwx5715_flash_data,
	},
	.num_resources	= 1,
	.resource	= &gtwx5715_flash_resource,
};

static struct platform_device *gtwx5715_devices[] __initdata = {
	&gtwx5715_uart_device,
	&gtwx5715_flash,
};

static void __init gtwx5715_init(void)
{
	ixp4xx_sys_init();

	gtwx5715_flash_resource.start = IXP4XX_EXP_BUS_BASE(0);
	gtwx5715_flash_resource.end = IXP4XX_EXP_BUS_BASE(0) + SZ_8M - 1;

	platform_add_devices(gtwx5715_devices, ARRAY_SIZE(gtwx5715_devices));
}


MACHINE_START(GTWX5715, "Gemtek GTWX5715 (Linksys WRV54G)")
	/* Maintainer: George Joseph */
	.phys_io	= IXP4XX_UART2_BASE_PHYS,
	.io_pg_offst	= ((IXP4XX_UART2_BASE_VIRT) >> 18) & 0xfffc,
	.map_io		= ixp4xx_map_io,
	.init_irq	= ixp4xx_init_irq,
	.timer		= &ixp4xx_timer,
	.boot_params	= 0x0100,
	.init_machine	= gtwx5715_init,
MACHINE_END


