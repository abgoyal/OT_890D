

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <mtd/mtd-abi.h>

static struct gpio_keys_button mtx1_gpio_button[] = {
	{
		.gpio = 207,
		.code = BTN_0,
		.desc = "System button",
	}
};

static struct gpio_keys_platform_data mtx1_buttons_data = {
	.buttons = mtx1_gpio_button,
	.nbuttons = ARRAY_SIZE(mtx1_gpio_button),
};

static struct platform_device mtx1_button = {
	.name = "gpio-keys",
	.id = -1,
	.dev = {
		.platform_data = &mtx1_buttons_data,
	}
};

static struct resource mtx1_wdt_res[] = {
	[0] = {
		.start	= 15,
		.end	= 15,
		.name	= "mtx1-wdt-gpio",
		.flags	= IORESOURCE_IRQ,
	}
};

static struct platform_device mtx1_wdt = {
	.name = "mtx1-wdt",
	.id = 0,
	.num_resources = ARRAY_SIZE(mtx1_wdt_res),
	.resource = mtx1_wdt_res,
};

static struct gpio_led default_leds[] = {
	{
		.name	= "mtx1:green",
		.gpio = 211,
	}, {
		.name = "mtx1:red",
		.gpio = 212,
	},
};

static struct gpio_led_platform_data mtx1_led_data = {
	.num_leds = ARRAY_SIZE(default_leds),
	.leds = default_leds,
};

static struct platform_device mtx1_gpio_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev = {
		.platform_data = &mtx1_led_data,
	}
};

static struct mtd_partition mtx1_mtd_partitions[] = {
	{
		.name	= "filesystem",
		.size	= 0x01C00000,
		.offset	= 0,
	},
	{
		.name	= "yamon",
		.size	= 0x00100000,
		.offset	= MTDPART_OFS_APPEND,
		.mask_flags = MTD_WRITEABLE,
	},
	{
		.name	= "kernel",
		.size	= 0x002c0000,
		.offset	= MTDPART_OFS_APPEND,
	},
	{
		.name	= "yamon env",
		.size	= 0x00040000,
		.offset	= MTDPART_OFS_APPEND,
	},
};

static struct physmap_flash_data mtx1_flash_data = {
	.width		= 4,
	.nr_parts	= 4,
	.parts		= mtx1_mtd_partitions,
};

static struct resource mtx1_mtd_resource = {
	.start	= 0x1e000000,
	.end	= 0x1fffffff,
	.flags	= IORESOURCE_MEM,
};

static struct platform_device mtx1_mtd = {
	.name		= "physmap-flash",
	.dev		= {
		.platform_data	= &mtx1_flash_data,
	},
	.num_resources	= 1,
	.resource	= &mtx1_mtd_resource,
};

static struct __initdata platform_device * mtx1_devs[] = {
	&mtx1_gpio_leds,
	&mtx1_wdt,
	&mtx1_button,
	&mtx1_mtd,
};

static int __init mtx1_register_devices(void)
{
	gpio_direction_input(207);
	return platform_add_devices(mtx1_devs, ARRAY_SIZE(mtx1_devs));
}

arch_initcall(mtx1_register_devices);
