

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/sysdev.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <asm/proc-fns.h>
#include <asm/irq.h>

#include <mach/reset.h>
#include <mach/idle.h>

#include <plat/cpu-freq.h>

#include <mach/regs-clock.h>
#include <plat/regs-serial.h>
#include <mach/regs-power.h>
#include <mach/regs-gpio.h>
#include <mach/regs-gpioj.h>
#include <mach/regs-dsc.h>
#include <plat/regs-spi.h>
#include <mach/regs-s3c2412.h>

#include <plat/s3c2412.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/clock.h>
#include <plat/pm.h>
#include <plat/pll.h>

#ifndef CONFIG_CPU_S3C2412_ONLY
void __iomem *s3c24xx_va_gpio2 = S3C24XX_VA_GPIO;

static inline void s3c2412_init_gpio2(void)
{
	s3c24xx_va_gpio2 = S3C24XX_VA_GPIO + 0x10;
}
#else
#define s3c2412_init_gpio2() do { } while(0)
#endif

/* Initial IO mappings */

static struct map_desc s3c2412_iodesc[] __initdata = {
	IODESC_ENT(CLKPWR),
	IODESC_ENT(TIMER),
	IODESC_ENT(WATCHDOG),
};

/* uart registration process */

void __init s3c2412_init_uarts(struct s3c2410_uartcfg *cfg, int no)
{
	s3c24xx_init_uartdevs("s3c2412-uart", s3c2410_uart_resources, cfg, no);

	/* rename devices that are s3c2412/s3c2413 specific */
	s3c_device_sdi.name  = "s3c2412-sdi";
	s3c_device_lcd.name  = "s3c2412-lcd";
	s3c_device_nand.name = "s3c2412-nand";

	/* alter IRQ of SDI controller */

	s3c_device_sdi.resource[1].start = IRQ_S3C2412_SDI;
	s3c_device_sdi.resource[1].end   = IRQ_S3C2412_SDI;

	/* spi channel related changes, s3c2412/13 specific */
	s3c_device_spi0.name = "s3c2412-spi";
	s3c_device_spi0.resource[0].end = S3C24XX_PA_SPI + 0x24;
	s3c_device_spi1.name = "s3c2412-spi";
	s3c_device_spi1.resource[0].start = S3C24XX_PA_SPI + S3C2412_SPI1;
	s3c_device_spi1.resource[0].end = S3C24XX_PA_SPI + S3C2412_SPI1 + 0x24;

}


static void s3c2412_idle(void)
{
	unsigned long tmp;

	/* ensure our idle mode is to go to idle */

	tmp = __raw_readl(S3C2412_PWRCFG);
	tmp &= ~S3C2412_PWRCFG_STANDBYWFI_MASK;
	tmp |= S3C2412_PWRCFG_STANDBYWFI_IDLE;
	__raw_writel(tmp, S3C2412_PWRCFG);

	cpu_do_idle();
}

static void s3c2412_hard_reset(void)
{
	/* errata "Watch-dog/Software Reset Problem" specifies that
	 * this reset must be done with the SYSCLK sourced from
	 * EXTCLK instead of FOUT to avoid a glitch in the reset
	 * mechanism.
	 *
	 * See the watchdog section of the S3C2412 manual for more
	 * information on this fix.
	 */

	__raw_writel(0x00, S3C2412_CLKSRC);
	__raw_writel(S3C2412_SWRST_RESET, S3C2412_SWRST);

	mdelay(1);
}


void __init s3c2412_map_io(void)
{
	/* move base of IO */

	s3c2412_init_gpio2();

	/* set our idle function */

	s3c24xx_idle = s3c2412_idle;

	/* set custom reset hook */

	s3c24xx_reset_hook = s3c2412_hard_reset;

	/* register our io-tables */

	iotable_init(s3c2412_iodesc, ARRAY_SIZE(s3c2412_iodesc));
}

void __init_or_cpufreq s3c2412_setup_clocks(void)
{
	struct clk *xtal_clk;
	unsigned long tmp;
	unsigned long xtal;
	unsigned long fclk;
	unsigned long hclk;
	unsigned long pclk;

	xtal_clk = clk_get(NULL, "xtal");
	xtal = clk_get_rate(xtal_clk);
	clk_put(xtal_clk);

	/* now we've got our machine bits initialised, work out what
	 * clocks we've got */

	fclk = s3c24xx_get_pll(__raw_readl(S3C2410_MPLLCON), xtal * 2);

	clk_mpll.rate = fclk;

	tmp = __raw_readl(S3C2410_CLKDIVN);

	/* work out clock scalings */

	hclk = fclk / ((tmp & S3C2412_CLKDIVN_HDIVN_MASK) + 1);
	hclk /= ((tmp & S3C2412_CLKDIVN_ARMDIVN) ? 2 : 1);
	pclk = hclk / ((tmp & S3C2412_CLKDIVN_PDIVN) ? 2 : 1);

	/* print brieft summary of clocks, etc */

	printk("S3C2412: core %ld.%03ld MHz, memory %ld.%03ld MHz, peripheral %ld.%03ld MHz\n",
	       print_mhz(fclk), print_mhz(hclk), print_mhz(pclk));

	s3c24xx_setup_clocks(fclk, hclk, pclk);
}

void __init s3c2412_init_clocks(int xtal)
{
	/* initialise the clocks here, to allow other things like the
	 * console to use them
	 */

	s3c24xx_register_baseclocks(xtal);
	s3c2412_setup_clocks();
	s3c2412_baseclk_add();
}


struct sysdev_class s3c2412_sysclass = {
	.name = "s3c2412-core",
};

static int __init s3c2412_core_init(void)
{
	return sysdev_class_register(&s3c2412_sysclass);
}

core_initcall(s3c2412_core_init);

static struct sys_device s3c2412_sysdev = {
	.cls		= &s3c2412_sysclass,
};

int __init s3c2412_init(void)
{
	printk("S3C2412: Initialising architecture\n");

	return sysdev_register(&s3c2412_sysdev);
}
