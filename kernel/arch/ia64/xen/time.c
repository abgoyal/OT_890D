

#include <linux/delay.h>
#include <linux/kernel_stat.h>
#include <linux/posix-timers.h>
#include <linux/irq.h>
#include <linux/clocksource.h>

#include <asm/timex.h>

#include <asm/xen/hypervisor.h>

#include <xen/interface/vcpu.h>

#include "../kernel/fsyscall_gtod_data.h"

DEFINE_PER_CPU(struct vcpu_runstate_info, runstate);
DEFINE_PER_CPU(unsigned long, processed_stolen_time);
DEFINE_PER_CPU(unsigned long, processed_blocked_time);

/* taken from i386/kernel/time-xen.c */
static void xen_init_missing_ticks_accounting(int cpu)
{
	struct vcpu_register_runstate_memory_area area;
	struct vcpu_runstate_info *runstate = &per_cpu(runstate, cpu);
	int rc;

	memset(runstate, 0, sizeof(*runstate));

	area.addr.v = runstate;
	rc = HYPERVISOR_vcpu_op(VCPUOP_register_runstate_memory_area, cpu,
				&area);
	WARN_ON(rc && rc != -ENOSYS);

	per_cpu(processed_blocked_time, cpu) = runstate->time[RUNSTATE_blocked];
	per_cpu(processed_stolen_time, cpu) = runstate->time[RUNSTATE_runnable]
					    + runstate->time[RUNSTATE_offline];
}

/* stolen from arch/x86/xen/time.c */
static void get_runstate_snapshot(struct vcpu_runstate_info *res)
{
	u64 state_time;
	struct vcpu_runstate_info *state;

	BUG_ON(preemptible());

	state = &__get_cpu_var(runstate);

	/*
	 * The runstate info is always updated by the hypervisor on
	 * the current CPU, so there's no need to use anything
	 * stronger than a compiler barrier when fetching it.
	 */
	do {
		state_time = state->state_entry_time;
		rmb();
		*res = *state;
		rmb();
	} while (state->state_entry_time != state_time);
}

#define NS_PER_TICK (1000000000LL/HZ)

static unsigned long
consider_steal_time(unsigned long new_itm)
{
	unsigned long stolen, blocked;
	unsigned long delta_itm = 0, stolentick = 0;
	int cpu = smp_processor_id();
	struct vcpu_runstate_info runstate;
	struct task_struct *p = current;

	get_runstate_snapshot(&runstate);

	/*
	 * Check for vcpu migration effect
	 * In this case, itc value is reversed.
	 * This causes huge stolen value.
	 * This function just checks and reject this effect.
	 */
	if (!time_after_eq(runstate.time[RUNSTATE_blocked],
			   per_cpu(processed_blocked_time, cpu)))
		blocked = 0;

	if (!time_after_eq(runstate.time[RUNSTATE_runnable] +
			   runstate.time[RUNSTATE_offline],
			   per_cpu(processed_stolen_time, cpu)))
		stolen = 0;

	if (!time_after(delta_itm + new_itm, ia64_get_itc()))
		stolentick = ia64_get_itc() - new_itm;

	do_div(stolentick, NS_PER_TICK);
	stolentick++;

	do_div(stolen, NS_PER_TICK);

	if (stolen > stolentick)
		stolen = stolentick;

	stolentick -= stolen;
	do_div(blocked, NS_PER_TICK);

	if (blocked > stolentick)
		blocked = stolentick;

	if (stolen > 0 || blocked > 0) {
		account_steal_ticks(stolen);
		account_idle_ticks(blocked);
		run_local_timers();

		if (rcu_pending(cpu))
			rcu_check_callbacks(cpu, user_mode(get_irq_regs()));

		scheduler_tick();
		run_posix_cpu_timers(p);
		delta_itm += local_cpu_data->itm_delta * (stolen + blocked);

		if (cpu == time_keeper_id) {
			write_seqlock(&xtime_lock);
			do_timer(stolen + blocked);
			local_cpu_data->itm_next = delta_itm + new_itm;
			write_sequnlock(&xtime_lock);
		} else {
			local_cpu_data->itm_next = delta_itm + new_itm;
		}
		per_cpu(processed_stolen_time, cpu) += NS_PER_TICK * stolen;
		per_cpu(processed_blocked_time, cpu) += NS_PER_TICK * blocked;
	}
	return delta_itm;
}

static int xen_do_steal_accounting(unsigned long *new_itm)
{
	unsigned long delta_itm;
	delta_itm = consider_steal_time(*new_itm);
	*new_itm += delta_itm;
	if (time_after(*new_itm, ia64_get_itc()) && delta_itm)
		return 1;

	return 0;
}

static void xen_itc_jitter_data_reset(void)
{
	u64 lcycle, ret;

	do {
		lcycle = itc_jitter_data.itc_lastcycle;
		ret = cmpxchg(&itc_jitter_data.itc_lastcycle, lcycle, 0);
	} while (unlikely(ret != lcycle));
}

struct pv_time_ops xen_time_ops __initdata = {
	.init_missing_ticks_accounting	= xen_init_missing_ticks_accounting,
	.do_steal_accounting		= xen_do_steal_accounting,
	.clocksource_resume		= xen_itc_jitter_data_reset,
};

/* Called after suspend, to resume time.  */
static void xen_local_tick_resume(void)
{
	/* Just trigger a tick.  */
	ia64_cpu_local_tick();
	touch_softlockup_watchdog();
}

void
xen_timer_resume(void)
{
	unsigned int cpu;

	xen_local_tick_resume();

	for_each_online_cpu(cpu)
		xen_init_missing_ticks_accounting(cpu);
}

static void ia64_cpu_local_tick_fn(void *unused)
{
	xen_local_tick_resume();
	xen_init_missing_ticks_accounting(smp_processor_id());
}

void
xen_timer_resume_on_aps(void)
{
	smp_call_function(&ia64_cpu_local_tick_fn, NULL, 1);
}
