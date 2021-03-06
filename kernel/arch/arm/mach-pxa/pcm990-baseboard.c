

#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/pwm_backlight.h>

#include <media/soc_camera.h>

#include <asm/gpio.h>
#include <mach/i2c.h>
#include <mach/camera.h>
#include <asm/mach/map.h>
#include <mach/pxa-regs.h>
#include <mach/audio.h>
#include <mach/mmc.h>
#include <mach/ohci.h>
#include <mach/pcm990_baseboard.h>
#include <mach/pxafb.h>
#include <mach/mfp-pxa27x.h>

#include "devices.h"
#include "generic.h"

static unsigned long pcm990_pin_config[] __initdata = {
	/* MMC */
	GPIO32_MMC_CLK,
	GPIO112_MMC_CMD,
	GPIO92_MMC_DAT_0,
	GPIO109_MMC_DAT_1,
	GPIO110_MMC_DAT_2,
	GPIO111_MMC_DAT_3,
	/* USB */
	GPIO88_USBH1_PWR,
	GPIO89_USBH1_PEN,
	/* PWM0 */
	GPIO16_PWM0_OUT,

	/* I2C */
	GPIO117_I2C_SCL,
	GPIO118_I2C_SDA,
};

#ifndef CONFIG_PCM990_DISPLAY_NONE
static void pcm990_lcd_power(int on, struct fb_var_screeninfo *var)
{
	if (on) {
		/* enable LCD-Latches
		 * power on LCD
		 */
		__PCM990_CTRL_REG(PCM990_CTRL_PHYS + PCM990_CTRL_REG3) =
			PCM990_CTRL_LCDPWR + PCM990_CTRL_LCDON;
	} else {
		/* disable LCD-Latches
		 * power off LCD
		 */
		__PCM990_CTRL_REG(PCM990_CTRL_PHYS + PCM990_CTRL_REG3) = 0x00;
	}
}
#endif

#if defined(CONFIG_PCM990_DISPLAY_SHARP)
static struct pxafb_mode_info fb_info_sharp_lq084v1dg21 = {
	.pixclock		= 28000,
	.xres			= 640,
	.yres			= 480,
	.bpp			= 16,
	.hsync_len		= 20,
	.left_margin		= 103,
	.right_margin		= 47,
	.vsync_len		= 6,
	.upper_margin		= 28,
	.lower_margin		= 5,
	.sync			= 0,
	.cmap_greyscale		= 0,
};

static struct pxafb_mach_info pcm990_fbinfo __initdata = {
	.modes			= &fb_info_sharp_lq084v1dg21,
	.num_modes		= 1,
	.lcd_conn		= LCD_COLOR_TFT_16BPP | LCD_PCLK_EDGE_FALL,
	.pxafb_lcd_power	= pcm990_lcd_power,
};
#elif defined(CONFIG_PCM990_DISPLAY_NEC)
struct pxafb_mode_info fb_info_nec_nl6448bc20_18d = {
	.pixclock		= 39720,
	.xres			= 640,
	.yres			= 480,
	.bpp			= 16,
	.hsync_len		= 32,
	.left_margin		= 16,
	.right_margin		= 48,
	.vsync_len		= 2,
	.upper_margin		= 12,
	.lower_margin		= 17,
	.sync			= 0,
	.cmap_greyscale		= 0,
};

static struct pxafb_mach_info pcm990_fbinfo __initdata = {
	.modes			= &fb_info_nec_nl6448bc20_18d,
	.num_modes		= 1,
	.lcd_conn		= LCD_COLOR_TFT_16BPP | LCD_PCLK_EDGE_FALL,
	.pxafb_lcd_power	= pcm990_lcd_power,
};
#endif

static struct platform_pwm_backlight_data pcm990_backlight_data = {
	.pwm_id		= 0,
	.max_brightness	= 1023,
	.dft_brightness	= 1023,
	.pwm_period_ns	= 78770,
};

static struct platform_device pcm990_backlight_device = {
	.name		= "pwm-backlight",
	.dev		= {
		.parent = &pxa27x_device_pwm0.dev,
		.platform_data = &pcm990_backlight_data,
	},
};


static unsigned long pcm990_irq_enabled;

static void pcm990_mask_ack_irq(unsigned int irq)
{
	int pcm990_irq = (irq - PCM027_IRQ(0));
	PCM990_INTMSKENA = (pcm990_irq_enabled &= ~(1 << pcm990_irq));
}

static void pcm990_unmask_irq(unsigned int irq)
{
	int pcm990_irq = (irq - PCM027_IRQ(0));
	/* the irq can be acknowledged only if deasserted, so it's done here */
	PCM990_INTSETCLR |= 1 << pcm990_irq;
	PCM990_INTMSKENA  = (pcm990_irq_enabled |= (1 << pcm990_irq));
}

static struct irq_chip pcm990_irq_chip = {
	.mask_ack	= pcm990_mask_ack_irq,
	.unmask		= pcm990_unmask_irq,
};

static void pcm990_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	unsigned long pending = (~PCM990_INTSETCLR) & pcm990_irq_enabled;

	do {
		GEDR(PCM990_CTRL_INT_IRQ_GPIO) =
					GPIO_bit(PCM990_CTRL_INT_IRQ_GPIO);
		if (likely(pending)) {
			irq = PCM027_IRQ(0) + __ffs(pending);
			generic_handle_irq(irq);
		}
		pending = (~PCM990_INTSETCLR) & pcm990_irq_enabled;
	} while (pending);
}

static void __init pcm990_init_irq(void)
{
	int irq;

	/* setup extra PCM990 irqs */
	for (irq = PCM027_IRQ(0); irq <= PCM027_IRQ(3); irq++) {
		set_irq_chip(irq, &pcm990_irq_chip);
		set_irq_handler(irq, handle_level_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	PCM990_INTMSKENA = 0x00;	/* disable all Interrupts */
	PCM990_INTSETCLR = 0xFF;

	set_irq_chained_handler(PCM990_CTRL_INT_IRQ, pcm990_irq_handler);
	set_irq_type(PCM990_CTRL_INT_IRQ, PCM990_CTRL_INT_IRQ_EDGE);
}

static int pcm990_mci_init(struct device *dev, irq_handler_t mci_detect_int,
			void *data)
{
	int err;

	err = request_irq(PCM027_MMCDET_IRQ, mci_detect_int, IRQF_DISABLED,
			     "MMC card detect", data);
	if (err)
		printk(KERN_ERR "pcm990_mci_init: MMC/SD: can't request MMC "
				"card detect IRQ\n");

	return err;
}

static void pcm990_mci_setpower(struct device *dev, unsigned int vdd)
{
	struct pxamci_platform_data *p_d = dev->platform_data;

	if ((1 << vdd) & p_d->ocr_mask)
		__PCM990_CTRL_REG(PCM990_CTRL_PHYS + PCM990_CTRL_REG5) =
						PCM990_CTRL_MMC2PWR;
	else
		__PCM990_CTRL_REG(PCM990_CTRL_PHYS + PCM990_CTRL_REG5) =
						~PCM990_CTRL_MMC2PWR;
}

static void pcm990_mci_exit(struct device *dev, void *data)
{
	free_irq(PCM027_MMCDET_IRQ, data);
}

#define MSECS_PER_JIFFY (1000/HZ)

static struct pxamci_platform_data pcm990_mci_platform_data = {
	.detect_delay	= 250 / MSECS_PER_JIFFY,
	.ocr_mask	= MMC_VDD_32_33 | MMC_VDD_33_34,
	.init 		= pcm990_mci_init,
	.setpower 	= pcm990_mci_setpower,
	.exit		= pcm990_mci_exit,
};

static struct pxaohci_platform_data pcm990_ohci_platform_data = {
	.port_mode	= PMM_PERPORT_MODE,
	.flags		= ENABLE_PORT1 | POWER_CONTROL_LOW | POWER_SENSE_LOW,
	.power_on_delay	= 10,
};

#if defined(CONFIG_VIDEO_PXA27x) || defined(CONFIG_VIDEO_PXA27x_MODULE)
static unsigned long pcm990_camera_pin_config[] = {
	/* CIF */
	GPIO98_CIF_DD_0,
	GPIO105_CIF_DD_1,
	GPIO104_CIF_DD_2,
	GPIO103_CIF_DD_3,
	GPIO95_CIF_DD_4,
	GPIO94_CIF_DD_5,
	GPIO93_CIF_DD_6,
	GPIO108_CIF_DD_7,
	GPIO107_CIF_DD_8,
	GPIO106_CIF_DD_9,
	GPIO42_CIF_MCLK,
	GPIO45_CIF_PCLK,
	GPIO43_CIF_FV,
	GPIO44_CIF_LV,
};

static int pcm990_pxacamera_init(struct device *dev)
{
	pxa2xx_mfp_config(ARRAY_AND_SIZE(pcm990_camera_pin_config));
	return 0;
}

struct pxacamera_platform_data pcm990_pxacamera_platform_data = {
	.init	= pcm990_pxacamera_init,
	.flags  = PXA_CAMERA_MASTER | PXA_CAMERA_DATAWIDTH_8 | PXA_CAMERA_DATAWIDTH_10 |
		PXA_CAMERA_PCLK_EN | PXA_CAMERA_MCLK_EN/* | PXA_CAMERA_PCP*/,
	.mclk_10khz = 1000,
};

#include <linux/i2c/pca953x.h>

static struct pca953x_platform_data pca9536_data = {
	.gpio_base	= NR_BUILTIN_GPIO + 1,
};

static struct soc_camera_link iclink[] = {
	{
		.bus_id	= 0, /* Must match with the camera ID above */
		.gpio	= NR_BUILTIN_GPIO + 1,
	}, {
		.bus_id	= 0, /* Must match with the camera ID above */
		.gpio	= -ENXIO,
	}
};

/* Board I2C devices. */
static struct i2c_board_info __initdata pcm990_i2c_devices[] = {
	{
		/* Must initialize before the camera(s) */
		I2C_BOARD_INFO("pca9536", 0x41),
		.platform_data = &pca9536_data,
	}, {
		I2C_BOARD_INFO("mt9v022", 0x48),
		.platform_data = &iclink[0], /* With extender */
	}, {
		I2C_BOARD_INFO("mt9m001", 0x5d),
		.platform_data = &iclink[0], /* With extender */
	},
};
#endif /* CONFIG_VIDEO_PXA27x ||CONFIG_VIDEO_PXA27x_MODULE */

static struct map_desc pcm990_io_desc[] __initdata = {
	{
		.virtual	= PCM990_CTRL_BASE,
		.pfn		= __phys_to_pfn(PCM990_CTRL_PHYS),
		.length		= PCM990_CTRL_SIZE,
		.type		= MT_DEVICE	/* CPLD */
	}, {
		.virtual	= PCM990_CF_PLD_BASE,
		.pfn		= __phys_to_pfn(PCM990_CF_PLD_PHYS),
		.length		= PCM990_CF_PLD_SIZE,
		.type		= MT_DEVICE	/* CPLD */
	}
};

void __init pcm990_baseboard_init(void)
{
	pxa2xx_mfp_config(ARRAY_AND_SIZE(pcm990_pin_config));

	/* register CPLD access */
	iotable_init(ARRAY_AND_SIZE(pcm990_io_desc));

	/* register CPLD's IRQ controller */
	pcm990_init_irq();

#ifndef CONFIG_PCM990_DISPLAY_NONE
	set_pxa_fb_info(&pcm990_fbinfo);
#endif
	platform_device_register(&pcm990_backlight_device);

	/* MMC */
	pxa_set_mci_info(&pcm990_mci_platform_data);

	/* USB host */
	pxa_set_ohci_info(&pcm990_ohci_platform_data);

	pxa_set_i2c_info(NULL);
	pxa_set_ac97_info(NULL);

#if defined(CONFIG_VIDEO_PXA27x) || defined(CONFIG_VIDEO_PXA27x_MODULE)
	pxa_set_camera_info(&pcm990_pxacamera_platform_data);

	i2c_register_board_info(0, ARRAY_AND_SIZE(pcm990_i2c_devices));
#endif

	printk(KERN_INFO "PCM-990 Evaluation baseboard initialized\n");
}
