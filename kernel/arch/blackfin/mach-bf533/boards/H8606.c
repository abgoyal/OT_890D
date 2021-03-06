

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#if defined(CONFIG_USB_ISP1362_HCD) || defined(CONFIG_USB_ISP1362_HCD_MODULE)
#include <linux/usb/isp1362.h>
#endif
#include <linux/irq.h>

#include <asm/dma.h>
#include <asm/bfin5xx_spi.h>
#include <asm/reboot.h>
#include <asm/portmux.h>

const char bfin_board_name[] = "HV Sistemas H8606";

#if defined(CONFIG_RTC_DRV_BFIN) || defined(CONFIG_RTC_DRV_BFIN_MODULE)
static struct platform_device rtc_device = {
	.name = "rtc-bfin",
	.id   = -1,
};
#endif

 #if	defined(CONFIG_DM9000) || defined(CONFIG_DM9000_MODULE)
static struct resource dm9000_resources[] = {
	[0] = {
		.start	= 0x20300000,
		.end	= 0x20300002,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 0x20300004,
		.end	= 0x20300006,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= IRQ_PF10,
		.end	= IRQ_PF10,
		.flags	= (IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IRQF_SHARED | IRQF_TRIGGER_HIGH),
	},
};

static struct platform_device dm9000_device = {
    .id			= 0,
    .name		= "dm9000",
    .resource		= dm9000_resources,
    .num_resources	= ARRAY_SIZE(dm9000_resources),
};
#endif

#if defined(CONFIG_SMC91X) || defined(CONFIG_SMC91X_MODULE)
static struct resource smc91x_resources[] = {
	{
		.name = "smc91x-regs",
		.start = 0x20300300,
		.end = 0x20300300 + 16,
		.flags = IORESOURCE_MEM,
	}, {
		.start = IRQ_PROG_INTB,
		.end = IRQ_PROG_INTB,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	}, {
		.start = IRQ_PF7,
		.end = IRQ_PF7,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct platform_device smc91x_device = {
	.name = "smc91x",
	.id = 0,
	.num_resources = ARRAY_SIZE(smc91x_resources),
	.resource = smc91x_resources,
};
#endif

#if defined(CONFIG_USB_NET2272) || defined(CONFIG_USB_NET2272_MODULE)
static struct resource net2272_bfin_resources[] = {
	{
		.start = 0x20300000,
		.end = 0x20300000 + 0x100,
		.flags = IORESOURCE_MEM,
	}, {
		.start = IRQ_PF10,
		.end = IRQ_PF10,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct platform_device net2272_bfin_device = {
	.name = "net2272",
	.id = -1,
	.num_resources = ARRAY_SIZE(net2272_bfin_resources),
	.resource = net2272_bfin_resources,
};
#endif

#if defined(CONFIG_SPI_BFIN) || defined(CONFIG_SPI_BFIN_MODULE)
/* all SPI peripherals info goes here */

#if defined(CONFIG_MTD_M25P80) || defined(CONFIG_MTD_M25P80_MODULE)
static struct mtd_partition bfin_spi_flash_partitions[] = {
	{
		.name = "bootloader (spi)",
		.size = 0x40000,
		.offset = 0,
		.mask_flags = MTD_CAP_ROM
	}, {
		.name = "fpga (spi)",
		.size =   0x30000,
		.offset = 0x40000
	}, {
		.name = "linux kernel (spi)",
		.size =   0x150000,
		.offset =  0x70000
	}, {
		.name = "jffs2 root file system (spi)",
		.size =   0x640000,
		.offset = 0x1c0000,
	}
};

static struct flash_platform_data bfin_spi_flash_data = {
	.name = "m25p80",
	.parts = bfin_spi_flash_partitions,
	.nr_parts = ARRAY_SIZE(bfin_spi_flash_partitions),
	.type = "m25p64",
};

/* SPI flash chip (m25p64) */
static struct bfin5xx_spi_chip spi_flash_chip_info = {
	.enable_dma = 0,         /* use dma transfer with this chip*/
	.bits_per_word = 8,
};
#endif

#if defined(CONFIG_SPI_ADC_BF533) || defined(CONFIG_SPI_ADC_BF533_MODULE)
/* SPI ADC chip */
static struct bfin5xx_spi_chip spi_adc_chip_info = {
	.ctl_reg = 0x1000,
	.enable_dma = 1,         /* use dma transfer with this chip*/
	.bits_per_word = 16,
};
#endif

#if defined(CONFIG_SND_BLACKFIN_AD1836) || defined(CONFIG_SND_BLACKFIN_AD1836_MODULE)
static struct bfin5xx_spi_chip ad1836_spi_chip_info = {
	.ctl_reg = 0x1000,
	.enable_dma = 0,
	.bits_per_word = 16,
};
#endif

#if defined(CONFIG_PBX)
static struct bfin5xx_spi_chip spi_si3xxx_chip_info = {
	.ctl_reg	= 0x1c04,
	.enable_dma	= 0,
	.bits_per_word	= 8,
	.cs_change_per_word = 1,
};
#endif

static struct spi_board_info bfin_spi_board_info[] __initdata = {
#if defined(CONFIG_MTD_M25P80) || defined(CONFIG_MTD_M25P80_MODULE)
	{
		/* the modalias must be the same as spi device driver name */
		.modalias = "m25p80", /* Name of spi_driver for this device */
		/* this value is the baudrate divisor */
		.max_speed_hz = 50000000, /* actual baudrate is SCLK/(2xspeed_hz) */
		.bus_num = 0, /* Framework bus number */
		.chip_select = 2, /* Framework chip select. On STAMP537 it is SPISSEL2*/
		.platform_data = &bfin_spi_flash_data,
		.controller_data = &spi_flash_chip_info,
		.mode = SPI_MODE_3,
	},
#endif

#if defined(CONFIG_SPI_ADC_BF533) || defined(CONFIG_SPI_ADC_BF533_MODULE)
	{
		.modalias = "bfin_spi_adc", /* Name of spi_driver for this device */
		.max_speed_hz = 4,     /* actual baudrate is SCLK/(2xspeed_hz) */
		.bus_num = 1, /* Framework bus number */
		.chip_select = 1, /* Framework chip select. */
		.platform_data = NULL, /* No spi_driver specific config */
		.controller_data = &spi_adc_chip_info,
	},
#endif

#if defined(CONFIG_SND_BLACKFIN_AD1836) || defined(CONFIG_SND_BLACKFIN_AD1836_MODULE)
	{
		.modalias = "ad1836-spi",
		.max_speed_hz = 16,
		.bus_num = 1,
		.chip_select = CONFIG_SND_BLACKFIN_SPI_PFBIT,
		.controller_data = &ad1836_spi_chip_info,
	},
#endif

#if defined(CONFIG_PBX)
	{
		.modalias	 = "fxs-spi",
		.max_speed_hz	 = 4,
		.bus_num	 = 1,
		.chip_select	 = 3,
		.controller_data = &spi_si3xxx_chip_info,
	},

	{
		.modalias	 = "fxo-spi",
		.max_speed_hz	 = 4,
		.bus_num	 = 1,
		.chip_select	 = 2,
		.controller_data = &spi_si3xxx_chip_info,
	},
#endif
};

/* SPI (0) */
static struct resource bfin_spi0_resource[] = {
	[0] = {
		.start = SPI0_REGBASE,
		.end   = SPI0_REGBASE + 0xFF,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = CH_SPI,
		.end   = CH_SPI,
		.flags = IORESOURCE_IRQ,
	}
};


/* SPI controller data */
static struct bfin5xx_spi_master bfin_spi0_info = {
	.num_chipselect = 8,
	.enable_dma = 1,  /* master has the ability to do dma transfer */
	.pin_req = {P_SPI0_SCK, P_SPI0_MISO, P_SPI0_MOSI, 0},
};

static struct platform_device bfin_spi0_device = {
	.name = "bfin-spi",
	.id = 0, /* Bus number */
	.num_resources = ARRAY_SIZE(bfin_spi0_resource),
	.resource = bfin_spi0_resource,
	.dev = {
		.platform_data = &bfin_spi0_info, /* Passed to driver */
	},
};
#endif  /* spi master and devices */

#if defined(CONFIG_FB_BF537_LQ035) || defined(CONFIG_FB_BF537_LQ035_MODULE)
static struct platform_device bfin_fb_device = {
	.name = "bf537-fb",
};
#endif

#if defined(CONFIG_SERIAL_BFIN) || defined(CONFIG_SERIAL_BFIN_MODULE)
static struct resource bfin_uart_resources[] = {
	{
		.start = 0xFFC00400,
		.end = 0xFFC004FF,
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device bfin_uart_device = {
	.name = "bfin-uart",
	.id = 1,
	.num_resources = ARRAY_SIZE(bfin_uart_resources),
	.resource = bfin_uart_resources,
};
#endif

#if defined(CONFIG_BFIN_SIR) || defined(CONFIG_BFIN_SIR_MODULE)
#ifdef CONFIG_BFIN_SIR0
static struct resource bfin_sir0_resources[] = {
	{
		.start = 0xFFC00400,
		.end = 0xFFC004FF,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = IRQ_UART0_RX,
		.end = IRQ_UART0_RX+1,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = CH_UART0_RX,
		.end = CH_UART0_RX+1,
		.flags = IORESOURCE_DMA,
	},
};

static struct platform_device bfin_sir0_device = {
	.name = "bfin_sir",
	.id = 0,
	.num_resources = ARRAY_SIZE(bfin_sir0_resources),
	.resource = bfin_sir0_resources,
};
#endif
#endif

#if defined(CONFIG_SERIAL_8250) || defined(CONFIG_SERIAL_8250_MODULE)

#include <linux/serial_8250.h>
#include <linux/serial.h>


static struct plat_serial8250_port serial8250_platform_data [] = {
	{
		.membase = (void *)0x20200000,
		.mapbase = 0x20200000,
		.irq = IRQ_PF8,
		.flags = UPF_BOOT_AUTOCONF | UART_CONFIG_TYPE,
		.iotype = UPIO_MEM,
		.regshift = 1,
		.uartclk = 66666667,
	}, {
		.membase = (void *)0x20200010,
		.mapbase = 0x20200010,
		.irq = IRQ_PF8,
		.flags = UPF_BOOT_AUTOCONF | UART_CONFIG_TYPE,
		.iotype = UPIO_MEM,
		.regshift = 1,
		.uartclk = 66666667,
	}, {
	}
};

static struct platform_device serial8250_device = {
	.id		= PLAT8250_DEV_PLATFORM,
	.name		= "serial8250",
	.dev		= {
		.platform_data = serial8250_platform_data,
	},
};

#endif

#if defined(CONFIG_KEYBOARD_OPENCORES) || defined(CONFIG_KEYBOARD_OPENCORES_MODULE)


static struct resource opencores_kbd_resources[] = {
	[0] = {
		.start	= 0x20200030,
		.end	= 0x20300030 + 2,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_PF9,
		.end	= IRQ_PF9,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
};

static struct platform_device opencores_kbd_device = {
	.id		= -1,
	.name		= "opencores-kbd",
	.resource	= opencores_kbd_resources,
	.num_resources	= ARRAY_SIZE(opencores_kbd_resources),
};
#endif

static struct platform_device *h8606_devices[] __initdata = {
#if defined(CONFIG_RTC_DRV_BFIN) || defined(CONFIG_RTC_DRV_BFIN_MODULE)
	&rtc_device,
#endif

#if	defined(CONFIG_DM9000) || defined(CONFIG_DM9000_MODULE)
	&dm9000_device,
#endif

#if defined(CONFIG_SMC91X) || defined(CONFIG_SMC91X_MODULE)
	&smc91x_device,
#endif

#if defined(CONFIG_USB_NET2272) || defined(CONFIG_USB_NET2272_MODULE)
	&net2272_bfin_device,
#endif

#if defined(CONFIG_SPI_BFIN) || defined(CONFIG_SPI_BFIN_MODULE)
	&bfin_spi0_device,
#endif

#if defined(CONFIG_SERIAL_BFIN) || defined(CONFIG_SERIAL_BFIN_MODULE)
	&bfin_uart_device,
#endif

#if defined(CONFIG_SERIAL_8250) || defined(CONFIG_SERIAL_8250_MODULE)
	&serial8250_device,
#endif

#if defined(CONFIG_BFIN_SIR) || defined(CONFIG_BFIN_SIR_MODULE)
#ifdef CONFIG_BFIN_SIR0
	&bfin_sir0_device,
#endif
#endif

#if defined(CONFIG_KEYBOARD_OPENCORES) || defined(CONFIG_KEYBOARD_OPENCORES_MODULE)
	&opencores_kbd_device,
#endif
};

static int __init H8606_init(void)
{
	printk(KERN_INFO "HV Sistemas H8606 board support by http://www.hvsistemas.com\n");
	printk(KERN_INFO "%s(): registering device resources\n", __func__);
	platform_add_devices(h8606_devices, ARRAY_SIZE(h8606_devices));
#if defined(CONFIG_SPI_BFIN) || defined(CONFIG_SPI_BFIN_MODULE)
	spi_register_board_info(bfin_spi_board_info, ARRAY_SIZE(bfin_spi_board_info));
#endif
	return 0;
}

arch_initcall(H8606_init);
