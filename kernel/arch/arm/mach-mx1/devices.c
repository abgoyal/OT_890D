

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#include <mach/irqs.h>
#include <mach/hardware.h>

static struct resource imx_csi_resources[] = {
	[0] = {
		.start  = 0x00224000,
		.end    = 0x00224010,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = CSI_INT,
		.end    = CSI_INT,
		.flags  = IORESOURCE_IRQ,
	},
};

static u64 imx_csi_dmamask = 0xffffffffUL;

struct platform_device imx_csi_device = {
	.name           = "imx-csi",
	.id             = 0, /* This is used to put cameras on this interface */
	.dev		= {
		.dma_mask = &imx_csi_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.resource       = imx_csi_resources,
	.num_resources  = ARRAY_SIZE(imx_csi_resources),
};

static struct resource imx_i2c_resources[] = {
	[0] = {
		.start  = 0x00217000,
		.end    = 0x00217010,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = I2C_INT,
		.end    = I2C_INT,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device imx_i2c_device = {
	.name           = "imx-i2c",
	.id             = 0,
	.resource       = imx_i2c_resources,
	.num_resources  = ARRAY_SIZE(imx_i2c_resources),
};

static struct resource imx_uart1_resources[] = {
	[0] = {
		.start	= UART1_BASE_ADDR,
		.end	= UART1_BASE_ADDR + 0xD0,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= UART1_MINT_RX,
		.end	= UART1_MINT_RX,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= UART1_MINT_TX,
		.end	= UART1_MINT_TX,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= UART1_MINT_RTS,
		.end	= UART1_MINT_RTS,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device imx_uart1_device = {
	.name		= "imx-uart",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(imx_uart1_resources),
	.resource	= imx_uart1_resources,
};

static struct resource imx_uart2_resources[] = {
	[0] = {
		.start	= UART2_BASE_ADDR,
		.end	= UART2_BASE_ADDR + 0xD0,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= UART2_MINT_RX,
		.end	= UART2_MINT_RX,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= UART2_MINT_TX,
		.end	= UART2_MINT_TX,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= UART2_MINT_RTS,
		.end	= UART2_MINT_RTS,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device imx_uart2_device = {
	.name		= "imx-uart",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(imx_uart2_resources),
	.resource	= imx_uart2_resources,
};

static struct resource imx_rtc_resources[] = {
	[0] = {
		.start  = 0x00204000,
		.end    = 0x00204024,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = RTC_INT,
		.end    = RTC_INT,
		.flags  = IORESOURCE_IRQ,
	},
	[2] = {
		.start  = RTC_SAMINT,
		.end    = RTC_SAMINT,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device imx_rtc_device = {
	.name           = "rtc-imx",
	.id             = 0,
	.resource       = imx_rtc_resources,
	.num_resources  = ARRAY_SIZE(imx_rtc_resources),
};

static struct resource imx_wdt_resources[] = {
	[0] = {
		.start  = 0x00201000,
		.end    = 0x00201008,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = WDT_INT,
		.end    = WDT_INT,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device imx_wdt_device = {
	.name           = "imx-wdt",
	.id             = 0,
	.resource       = imx_wdt_resources,
	.num_resources  = ARRAY_SIZE(imx_wdt_resources),
};

static struct resource imx_usb_resources[] = {
	[0] = {
		.start	= 0x00212000,
		.end	= 0x00212148,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= USBD_INT0,
		.end	= USBD_INT0,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= USBD_INT1,
		.end	= USBD_INT1,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= USBD_INT2,
		.end	= USBD_INT2,
		.flags	= IORESOURCE_IRQ,
	},
	[4] = {
		.start	= USBD_INT3,
		.end	= USBD_INT3,
		.flags	= IORESOURCE_IRQ,
	},
	[5] = {
		.start	= USBD_INT4,
		.end	= USBD_INT4,
		.flags	= IORESOURCE_IRQ,
	},
	[6] = {
		.start	= USBD_INT5,
		.end	= USBD_INT5,
		.flags	= IORESOURCE_IRQ,
	},
	[7] = {
		.start	= USBD_INT6,
		.end	= USBD_INT6,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device imx_usb_device = {
	.name		= "imx_udc",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(imx_usb_resources),
	.resource	= imx_usb_resources,
};

/* GPIO port description */
static struct mxc_gpio_port imx_gpio_ports[] = {
	[0] = {
		.chip.label = "gpio-0",
		.base = (void __iomem *)IO_ADDRESS(GPIO_BASE_ADDR),
		.irq = GPIO_INT_PORTA,
		.virtual_irq_start = MXC_GPIO_IRQ_START
	},
	[1] = {
		.chip.label = "gpio-1",
		.base = (void __iomem *)IO_ADDRESS(GPIO_BASE_ADDR + 0x100),
		.irq = GPIO_INT_PORTB,
		.virtual_irq_start = MXC_GPIO_IRQ_START + 32
	},
	[2] = {
		.chip.label = "gpio-2",
		.base = (void __iomem *)IO_ADDRESS(GPIO_BASE_ADDR + 0x200),
		.irq = GPIO_INT_PORTC,
		.virtual_irq_start = MXC_GPIO_IRQ_START + 64
	},
	[3] = {
		.chip.label = "gpio-3",
		.base = (void __iomem *)IO_ADDRESS(GPIO_BASE_ADDR + 0x300),
		.irq = GPIO_INT_PORTD,
		.virtual_irq_start = MXC_GPIO_IRQ_START + 96
	}
};

int __init mxc_register_gpios(void)
{
	return mxc_gpio_init(imx_gpio_ports, ARRAY_SIZE(imx_gpio_ports));
}
