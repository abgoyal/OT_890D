
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/profile.h>
#include <linux/clocksource.h>
#include <linux/platform_device.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/param.h>
#include <asm/pdc.h>
#include <asm/led.h>

#include <linux/timex.h>

static unsigned long clocktick __read_mostly;	/* timer cycles per tick */

irqreturn_t timer_interrupt(int irq, void *dev_id)
{
	unsigned long now;
	unsigned long next_tick;
	unsigned long cycles_elapsed, ticks_elapsed;
	unsigned long cycles_remainder;
	unsigned int cpu = smp_processor_id();
	struct cpuinfo_parisc *cpuinfo = &per_cpu(cpu_data, cpu);

	/* gcc can optimize for "read-only" case with a local clocktick */
	unsigned long cpt = clocktick;

	profile_tick(CPU_PROFILING);

	/* Initialize next_tick to the expected tick time. */
	next_tick = cpuinfo->it_value;

	/* Get current interval timer.
	 * CR16 reads as 64 bits in CPU wide mode.
	 * CR16 reads as 32 bits in CPU narrow mode.
	 */
	now = mfctl(16);

	cycles_elapsed = now - next_tick;

	if ((cycles_elapsed >> 5) < cpt) {
		/* use "cheap" math (add/subtract) instead
		 * of the more expensive div/mul method
		 */
		cycles_remainder = cycles_elapsed;
		ticks_elapsed = 1;
		while (cycles_remainder > cpt) {
			cycles_remainder -= cpt;
			ticks_elapsed++;
		}
	} else {
		cycles_remainder = cycles_elapsed % cpt;
		ticks_elapsed = 1 + cycles_elapsed / cpt;
	}

	/* Can we differentiate between "early CR16" (aka Scenario 1) and
	 * "long delay" (aka Scenario 3)? I don't think so.
	 *
	 * We expected timer_interrupt to be delivered at least a few hundred
	 * cycles after the IT fires. But it's arbitrary how much time passes
	 * before we call it "late". I've picked one second.
	 */
	if (unlikely(ticks_elapsed > HZ)) {
		/* Scenario 3: very long delay?  bad in any case */
		printk (KERN_CRIT "timer_interrupt(CPU %d): delayed!"
			" cycles %lX rem %lX "
			" next/now %lX/%lX\n",
			cpu,
			cycles_elapsed, cycles_remainder,
			next_tick, now );
	}

	/* convert from "division remainder" to "remainder of clock tick" */
	cycles_remainder = cpt - cycles_remainder;

	/* Determine when (in CR16 cycles) next IT interrupt will fire.
	 * We want IT to fire modulo clocktick even if we miss/skip some.
	 * But those interrupts don't in fact get delivered that regularly.
	 */
	next_tick = now + cycles_remainder;

	cpuinfo->it_value = next_tick;

	/* Skip one clocktick on purpose if we are likely to miss next_tick.
	 * We want to avoid the new next_tick being less than CR16.
	 * If that happened, itimer wouldn't fire until CR16 wrapped.
	 * We'll catch the tick we missed on the tick after that.
	 */
	if (!(cycles_remainder >> 13))
		next_tick += cpt;

	/* Program the IT when to deliver the next interrupt. */
	/* Only bottom 32-bits of next_tick are written to cr16.  */
	mtctl(next_tick, 16);


	/* Done mucking with unreliable delivery of interrupts.
	 * Go do system house keeping.
	 */

	if (!--cpuinfo->prof_counter) {
		cpuinfo->prof_counter = cpuinfo->prof_multiplier;
		update_process_times(user_mode(get_irq_regs()));
	}

	if (cpu == 0) {
		write_seqlock(&xtime_lock);
		do_timer(ticks_elapsed);
		write_sequnlock(&xtime_lock);
	}

	return IRQ_HANDLED;
}


unsigned long profile_pc(struct pt_regs *regs)
{
	unsigned long pc = instruction_pointer(regs);

	if (regs->gr[0] & PSW_N)
		pc -= 4;

#ifdef CONFIG_SMP
	if (in_lock_functions(pc))
		pc = regs->gr[2];
#endif

	return pc;
}
EXPORT_SYMBOL(profile_pc);


/* clock source code */

static cycle_t read_cr16(void)
{
	return get_cycles();
}

static struct clocksource clocksource_cr16 = {
	.name			= "cr16",
	.rating			= 300,
	.read			= read_cr16,
	.mask			= CLOCKSOURCE_MASK(BITS_PER_LONG),
	.mult			= 0, /* to be set */
	.shift			= 22,
	.flags			= CLOCK_SOURCE_IS_CONTINUOUS,
};

#ifdef CONFIG_SMP
int update_cr16_clocksource(void)
{
	/* since the cr16 cycle counters are not synchronized across CPUs,
	   we'll check if we should switch to a safe clocksource: */
	if (clocksource_cr16.rating != 0 && num_online_cpus() > 1) {
		clocksource_change_rating(&clocksource_cr16, 0);
		return 1;
	}

	return 0;
}
#else
int update_cr16_clocksource(void)
{
	return 0; /* no change */
}
#endif /*CONFIG_SMP*/

void __init start_cpu_itimer(void)
{
	unsigned int cpu = smp_processor_id();
	unsigned long next_tick = mfctl(16) + clocktick;

	mtctl(next_tick, 16);		/* kick off Interval Timer (CR16) */

	per_cpu(cpu_data, cpu).it_value = next_tick;
}

struct platform_device rtc_parisc_dev = {
	.name = "rtc-parisc",
	.id = -1,
};

static int __init rtc_init(void)
{
	int ret;

	ret = platform_device_register(&rtc_parisc_dev);
	if (ret < 0)
		printk(KERN_ERR "unable to register rtc device...\n");

	/* not necessarily an error */
	return 0;
}
module_init(rtc_init);

void __init time_init(void)
{
	static struct pdc_tod tod_data;
	unsigned long current_cr16_khz;

	clocktick = (100 * PAGE0->mem_10msec) / HZ;

	start_cpu_itimer();	/* get CPU 0 started */

	/* register at clocksource framework */
	current_cr16_khz = PAGE0->mem_10msec/10;  /* kHz */
	clocksource_cr16.mult = clocksource_khz2mult(current_cr16_khz,
						clocksource_cr16.shift);
	clocksource_register(&clocksource_cr16);

	if (pdc_tod_read(&tod_data) == 0) {
		unsigned long flags;

		write_seqlock_irqsave(&xtime_lock, flags);
		xtime.tv_sec = tod_data.tod_sec;
		xtime.tv_nsec = tod_data.tod_usec * 1000;
		set_normalized_timespec(&wall_to_monotonic,
		                        -xtime.tv_sec, -xtime.tv_nsec);
		write_sequnlock_irqrestore(&xtime_lock, flags);
	} else {
		printk(KERN_ERR "Error reading tod clock\n");
	        xtime.tv_sec = 0;
		xtime.tv_nsec = 0;
	}
}
