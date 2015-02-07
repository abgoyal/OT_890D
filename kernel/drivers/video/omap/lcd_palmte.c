

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include <mach/fpga.h>
#include <mach/omapfb.h>

static int palmte_panel_init(struct lcd_panel *panel,
				struct omapfb_device *fbdev)
{
	return 0;
}

static void palmte_panel_cleanup(struct lcd_panel *panel)
{
}

static int palmte_panel_enable(struct lcd_panel *panel)
{
	return 0;
}

static void palmte_panel_disable(struct lcd_panel *panel)
{
}

static unsigned long palmte_panel_get_caps(struct lcd_panel *panel)
{
	return 0;
}

struct lcd_panel palmte_panel = {
	.name		= "palmte",
	.config		= OMAP_LCDC_PANEL_TFT | OMAP_LCDC_INV_VSYNC |
			  OMAP_LCDC_INV_HSYNC | OMAP_LCDC_HSVS_RISING_EDGE |
			  OMAP_LCDC_HSVS_OPPOSITE,

	.data_lines	= 16,
	.bpp		= 8,
	.pixel_clock	= 12000,
	.x_res		= 320,
	.y_res		= 320,
	.hsw		= 4,
	.hfp		= 8,
	.hbp		= 28,
	.vsw		= 1,
	.vfp		= 8,
	.vbp		= 7,
	.pcd		= 0,

	.init		= palmte_panel_init,
	.cleanup	= palmte_panel_cleanup,
	.enable		= palmte_panel_enable,
	.disable	= palmte_panel_disable,
	.get_caps	= palmte_panel_get_caps,
};

static int palmte_panel_probe(struct platform_device *pdev)
{
	omapfb_register_panel(&palmte_panel);
	return 0;
}

static int palmte_panel_remove(struct platform_device *pdev)
{
	return 0;
}

static int palmte_panel_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int palmte_panel_resume(struct platform_device *pdev)
{
	return 0;
}

struct platform_driver palmte_panel_driver = {
	.probe		= palmte_panel_probe,
	.remove		= palmte_panel_remove,
	.suspend	= palmte_panel_suspend,
	.resume		= palmte_panel_resume,
	.driver		= {
		.name	= "lcd_palmte",
		.owner	= THIS_MODULE,
	},
};

static int palmte_panel_drv_init(void)
{
	return platform_driver_register(&palmte_panel_driver);
}

static void palmte_panel_drv_cleanup(void)
{
	platform_driver_unregister(&palmte_panel_driver);
}

module_init(palmte_panel_drv_init);
module_exit(palmte_panel_drv_cleanup);
