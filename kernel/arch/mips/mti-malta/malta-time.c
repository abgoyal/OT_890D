

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/mc146818rtc.h>

#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/hardirq.h>
#include <asm/i8253.h>
#include <asm/irq.h>
#include <asm/div64.h>
#include <asm/cpu.h>
#include <asm/time.h>
#include <asm/mc146818-time.h>
#include <asm/msc01_ic.h>

#include <asm/mips-boards/generic.h>
#include <asm/mips-boards/prom.h>

#include <asm/mips-boards/maltaint.h>

unsigned long cpu_khz;

static int mips_cpu_timer_irq;
static int mips_cpu_perf_irq;
extern int cp0_perfcount_irq;

static void mips_timer_dispatch(void)
{
	do_IRQ(mips_cpu_timer_irq);
}

static void mips_perf_dispatch(void)
{
	do_IRQ(mips_cpu_perf_irq);
}

static unsigned int __init estimate_cpu_frequency(void)
{
	unsigned int prid = read_c0_prid() & 0xffff00;
	unsigned int count;

	unsigned long flags;
	unsigned int start;

	local_irq_save(flags);

	/* Start counter exactly on falling edge of update flag */
	while (CMOS_READ(RTC_REG_A) & RTC_UIP);
	while (!(CMOS_READ(RTC_REG_A) & RTC_UIP));

	/* Start r4k counter. */
	start = read_c0_count();

	/* Read counter exactly on falling edge of update flag */
	while (CMOS_READ(RTC_REG_A) & RTC_UIP);
	while (!(CMOS_READ(RTC_REG_A) & RTC_UIP));

	count = read_c0_count() - start;

	/* restore interrupts */
	local_irq_restore(flags);

	mips_hpt_frequency = count;
	if ((prid != (PRID_COMP_MIPS | PRID_IMP_20KC)) &&
	    (prid != (PRID_COMP_MIPS | PRID_IMP_25KF)))
		count *= 2;

	count += 5000;    /* round */
	count -= count%10000;

	return count;
}

unsigned long read_persistent_clock(void)
{
	return mc146818_get_cmos_time();
}

static void __init plat_perf_setup(void)
{
#ifdef MSC01E_INT_BASE
	if (cpu_has_veic) {
		set_vi_handler(MSC01E_INT_PERFCTR, mips_perf_dispatch);
		mips_cpu_perf_irq = MSC01E_INT_BASE + MSC01E_INT_PERFCTR;
	} else
#endif
	if (cp0_perfcount_irq >= 0) {
		if (cpu_has_vint)
			set_vi_handler(cp0_perfcount_irq, mips_perf_dispatch);
		mips_cpu_perf_irq = MIPS_CPU_IRQ_BASE + cp0_perfcount_irq;
#ifdef CONFIG_SMP
		set_irq_handler(mips_cpu_perf_irq, handle_percpu_irq);
#endif
	}
}

unsigned int __cpuinit get_c0_compare_int(void)
{
#ifdef MSC01E_INT_BASE
	if (cpu_has_veic) {
		set_vi_handler(MSC01E_INT_CPUCTR, mips_timer_dispatch);
		mips_cpu_timer_irq = MSC01E_INT_BASE + MSC01E_INT_CPUCTR;
	} else
#endif
	{
		if (cpu_has_vint)
			set_vi_handler(cp0_compare_irq, mips_timer_dispatch);
		mips_cpu_timer_irq = MIPS_CPU_IRQ_BASE + cp0_compare_irq;
	}

	return mips_cpu_timer_irq;
}

void __init plat_time_init(void)
{
	unsigned int est_freq;

        /* Set Data mode - binary. */
        CMOS_WRITE(CMOS_READ(RTC_CONTROL) | RTC_DM_BINARY, RTC_CONTROL);

	est_freq = estimate_cpu_frequency();

	printk("CPU frequency %d.%02d MHz\n", est_freq/1000000,
	       (est_freq%1000000)*100/1000000);

        cpu_khz = est_freq / 1000;

	mips_scroll_message();
#ifdef CONFIG_I8253		/* Only Malta has a PIT */
	setup_pit_timer();
#endif

	plat_perf_setup();
}
