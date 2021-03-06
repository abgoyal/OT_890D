

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/interrupt.h>
#include <linux/nmi.h>
#include <linux/kexec.h>
#include <linux/debug_locks.h>
#include <linux/random.h>
#include <linux/kallsyms.h>
#include <linux/dmi.h>

int panic_on_oops;
static unsigned long tainted_mask;
static int pause_on_oops;
static int pause_on_oops_flag;
static DEFINE_SPINLOCK(pause_on_oops_lock);

#ifndef CONFIG_PANIC_TIMEOUT
#define CONFIG_PANIC_TIMEOUT 0
#endif
int panic_timeout = CONFIG_PANIC_TIMEOUT;

ATOMIC_NOTIFIER_HEAD(panic_notifier_list);

EXPORT_SYMBOL(panic_notifier_list);

static long no_blink(long time)
{
	return 0;
}

/* Returns how long it waited in ms */
long (*panic_blink)(long time);
EXPORT_SYMBOL(panic_blink);


NORET_TYPE void panic(const char * fmt, ...)
{
	long i;
	static char buf[1024];
	va_list args;
#if defined(CONFIG_S390)
	unsigned long caller = (unsigned long) __builtin_return_address(0);
#endif

	/*
	 * It's possible to come here directly from a panic-assertion and not
	 * have preempt disabled. Some functions called from here want
	 * preempt to be disabled. No point enabling it later though...
	 */
	preempt_disable();

	bust_spinlocks(1);
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	printk(KERN_EMERG "Kernel panic - not syncing: %s\n",buf);
	bust_spinlocks(0);

	/*
	 * If we have crashed and we have a crash kernel loaded let it handle
	 * everything else.
	 * Do we want to call this before we try to display a message?
	 */
	crash_kexec(NULL);

#ifdef CONFIG_SMP
	/*
	 * Note smp_send_stop is the usual smp shutdown function, which
	 * unfortunately means it may not be hardened to work in a panic
	 * situation.
	 */
	smp_send_stop();
#endif

	atomic_notifier_call_chain(&panic_notifier_list, 0, buf);

	if (!panic_blink)
		panic_blink = no_blink;

	if (panic_timeout > 0) {
		/*
	 	 * Delay timeout seconds before rebooting the machine. 
		 * We can't use the "normal" timers since we just panicked..
	 	 */
		printk(KERN_EMERG "Rebooting in %d seconds..",panic_timeout);
		for (i = 0; i < panic_timeout*1000; ) {
			touch_nmi_watchdog();
			i += panic_blink(i);
			mdelay(1);
			i++;
		}
		/*	This will not be a clean reboot, with everything
		 *	shutting down.  But if there is a chance of
		 *	rebooting the system it will be rebooted.
		 */
		emergency_restart();
	}
#ifdef __sparc__
	{
		extern int stop_a_enabled;
		/* Make sure the user can actually press Stop-A (L1-A) */
		stop_a_enabled = 1;
		printk(KERN_EMERG "Press Stop-A (L1-A) to return to the boot prom\n");
	}
#endif
#if defined(CONFIG_S390)
	disabled_wait(caller);
#endif
	local_irq_enable();
	for (i = 0;;) {
		touch_softlockup_watchdog();
		i += panic_blink(i);
		mdelay(1);
		i++;
	}
}

EXPORT_SYMBOL(panic);


struct tnt {
	u8 bit;
	char true;
	char false;
};

static const struct tnt tnts[] = {
	{ TAINT_PROPRIETARY_MODULE, 'P', 'G' },
	{ TAINT_FORCED_MODULE, 'F', ' ' },
	{ TAINT_UNSAFE_SMP, 'S', ' ' },
	{ TAINT_FORCED_RMMOD, 'R', ' ' },
	{ TAINT_MACHINE_CHECK, 'M', ' ' },
	{ TAINT_BAD_PAGE, 'B', ' ' },
	{ TAINT_USER, 'U', ' ' },
	{ TAINT_DIE, 'D', ' ' },
	{ TAINT_OVERRIDDEN_ACPI_TABLE, 'A', ' ' },
	{ TAINT_WARN, 'W', ' ' },
	{ TAINT_CRAP, 'C', ' ' },
};

const char *print_tainted(void)
{
	static char buf[ARRAY_SIZE(tnts) + sizeof("Tainted: ") + 1];

	if (tainted_mask) {
		char *s;
		int i;

		s = buf + sprintf(buf, "Tainted: ");
		for (i = 0; i < ARRAY_SIZE(tnts); i++) {
			const struct tnt *t = &tnts[i];
			*s++ = test_bit(t->bit, &tainted_mask) ?
					t->true : t->false;
		}
		*s = 0;
	} else
		snprintf(buf, sizeof(buf), "Not tainted");
	return(buf);
}

int test_taint(unsigned flag)
{
	return test_bit(flag, &tainted_mask);
}
EXPORT_SYMBOL(test_taint);

unsigned long get_taint(void)
{
	return tainted_mask;
}

void add_taint(unsigned flag)
{
	debug_locks = 0; /* can't trust the integrity of the kernel anymore */
	set_bit(flag, &tainted_mask);
}
EXPORT_SYMBOL(add_taint);

static void spin_msec(int msecs)
{
	int i;

	for (i = 0; i < msecs; i++) {
		touch_nmi_watchdog();
		mdelay(1);
	}
}

static void do_oops_enter_exit(void)
{
	unsigned long flags;
	static int spin_counter;

	if (!pause_on_oops)
		return;

	spin_lock_irqsave(&pause_on_oops_lock, flags);
	if (pause_on_oops_flag == 0) {
		/* This CPU may now print the oops message */
		pause_on_oops_flag = 1;
	} else {
		/* We need to stall this CPU */
		if (!spin_counter) {
			/* This CPU gets to do the counting */
			spin_counter = pause_on_oops;
			do {
				spin_unlock(&pause_on_oops_lock);
				spin_msec(MSEC_PER_SEC);
				spin_lock(&pause_on_oops_lock);
			} while (--spin_counter);
			pause_on_oops_flag = 0;
		} else {
			/* This CPU waits for a different one */
			while (spin_counter) {
				spin_unlock(&pause_on_oops_lock);
				spin_msec(1);
				spin_lock(&pause_on_oops_lock);
			}
		}
	}
	spin_unlock_irqrestore(&pause_on_oops_lock, flags);
}

int oops_may_print(void)
{
	return pause_on_oops_flag == 0;
}

void oops_enter(void)
{
	debug_locks_off(); /* can't trust the integrity of the kernel anymore */
	do_oops_enter_exit();
}

static u64 oops_id;

static int init_oops_id(void)
{
	if (!oops_id)
		get_random_bytes(&oops_id, sizeof(oops_id));
	else
		oops_id++;

	return 0;
}
late_initcall(init_oops_id);

static void print_oops_end_marker(void)
{
	init_oops_id();
	printk(KERN_WARNING "---[ end trace %016llx ]---\n",
		(unsigned long long)oops_id);
}

void oops_exit(void)
{
	do_oops_enter_exit();
	print_oops_end_marker();
}

#ifdef WANT_WARN_ON_SLOWPATH
void warn_slowpath(const char *file, int line, const char *fmt, ...)
{
	va_list args;
	char function[KSYM_SYMBOL_LEN];
	unsigned long caller = (unsigned long)__builtin_return_address(0);
	const char *board;

	sprint_symbol(function, caller);

	printk(KERN_WARNING "------------[ cut here ]------------\n");
	printk(KERN_WARNING "WARNING: at %s:%d %s()\n", file,
		line, function);
	board = dmi_get_system_info(DMI_PRODUCT_NAME);
	if (board)
		printk(KERN_WARNING "Hardware name: %s\n", board);

	if (fmt) {
		va_start(args, fmt);
		vprintk(fmt, args);
		va_end(args);
	}

	print_modules();
	dump_stack();
	print_oops_end_marker();
	add_taint(TAINT_WARN);
}
EXPORT_SYMBOL(warn_slowpath);
#endif

#ifdef CONFIG_CC_STACKPROTECTOR
void __stack_chk_fail(void)
{
	panic("stack-protector: Kernel stack is corrupted");
}
EXPORT_SYMBOL(__stack_chk_fail);
#endif

core_param(panic, panic_timeout, int, 0644);
core_param(pause_on_oops, pause_on_oops, int, 0644);
