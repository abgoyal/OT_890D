

#include <linux/sched.h>
#include <linux/list.h>
#include <linux/signal.h>
#include <linux/spinlock.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>		/* Needed by writeback.h	  */
#include <linux/writeback.h>	/* Prototypes pdflush_operation() */
#include <linux/kthread.h>
#include <linux/cpuset.h>
#include <linux/freezer.h>


#define MIN_PDFLUSH_THREADS	2
#define MAX_PDFLUSH_THREADS	8

static void start_one_pdflush_thread(void);



static LIST_HEAD(pdflush_list);
static DEFINE_SPINLOCK(pdflush_lock);

int nr_pdflush_threads = 0;

static unsigned long last_empty_jifs;


struct pdflush_work {
	struct task_struct *who;	/* The thread */
	void (*fn)(unsigned long);	/* A callback function */
	unsigned long arg0;		/* An argument to the callback */
	struct list_head list;		/* On pdflush_list, when idle */
	unsigned long when_i_went_to_sleep;
};

static int __pdflush(struct pdflush_work *my_work)
{
	current->flags |= PF_FLUSHER | PF_SWAPWRITE;
	set_freezable();
	my_work->fn = NULL;
	my_work->who = current;
	INIT_LIST_HEAD(&my_work->list);

	spin_lock_irq(&pdflush_lock);
	nr_pdflush_threads++;
	for ( ; ; ) {
		struct pdflush_work *pdf;

		set_current_state(TASK_INTERRUPTIBLE);
		list_move(&my_work->list, &pdflush_list);
		my_work->when_i_went_to_sleep = jiffies;
		spin_unlock_irq(&pdflush_lock);
		schedule();
		try_to_freeze();
		spin_lock_irq(&pdflush_lock);
		if (!list_empty(&my_work->list)) {
			/*
			 * Someone woke us up, but without removing our control
			 * structure from the global list.  swsusp will do this
			 * in try_to_freeze()->refrigerator().  Handle it.
			 */
			my_work->fn = NULL;
			continue;
		}
		if (my_work->fn == NULL) {
			printk("pdflush: bogus wakeup\n");
			continue;
		}
		spin_unlock_irq(&pdflush_lock);

		(*my_work->fn)(my_work->arg0);

		/*
		 * Thread creation: For how long have there been zero
		 * available threads?
		 */
		if (time_after(jiffies, last_empty_jifs + 1 * HZ)) {
			/* unlocked list_empty() test is OK here */
			if (list_empty(&pdflush_list)) {
				/* unlocked test is OK here */
				if (nr_pdflush_threads < MAX_PDFLUSH_THREADS)
					start_one_pdflush_thread();
			}
		}

		spin_lock_irq(&pdflush_lock);
		my_work->fn = NULL;

		/*
		 * Thread destruction: For how long has the sleepiest
		 * thread slept?
		 */
		if (list_empty(&pdflush_list))
			continue;
		if (nr_pdflush_threads <= MIN_PDFLUSH_THREADS)
			continue;
		pdf = list_entry(pdflush_list.prev, struct pdflush_work, list);
		if (time_after(jiffies, pdf->when_i_went_to_sleep + 1 * HZ)) {
			/* Limit exit rate */
			pdf->when_i_went_to_sleep = jiffies;
			break;					/* exeunt */
		}
	}
	nr_pdflush_threads--;
	spin_unlock_irq(&pdflush_lock);
	return 0;
}

static int pdflush(void *dummy)
{
	struct pdflush_work my_work;
	cpumask_var_t cpus_allowed;

	/*
	 * Since the caller doesn't even check kthread_run() worked, let's not
	 * freak out too much if this fails.
	 */
	if (!alloc_cpumask_var(&cpus_allowed, GFP_KERNEL)) {
		printk(KERN_WARNING "pdflush failed to allocate cpumask\n");
		return 0;
	}

	/*
	 * pdflush can spend a lot of time doing encryption via dm-crypt.  We
	 * don't want to do that at keventd's priority.
	 */
	set_user_nice(current, 0);

	/*
	 * Some configs put our parent kthread in a limited cpuset,
	 * which kthread() overrides, forcing cpus_allowed == CPU_MASK_ALL.
	 * Our needs are more modest - cut back to our cpusets cpus_allowed.
	 * This is needed as pdflush's are dynamically created and destroyed.
	 * The boottime pdflush's are easily placed w/o these 2 lines.
	 */
	cpuset_cpus_allowed(current, cpus_allowed);
	set_cpus_allowed_ptr(current, cpus_allowed);
	free_cpumask_var(cpus_allowed);

	return __pdflush(&my_work);
}

int pdflush_operation(void (*fn)(unsigned long), unsigned long arg0)
{
	unsigned long flags;
	int ret = 0;

	BUG_ON(fn == NULL);	/* Hard to diagnose if it's deferred */

	spin_lock_irqsave(&pdflush_lock, flags);
	if (list_empty(&pdflush_list)) {
		ret = -1;
	} else {
		struct pdflush_work *pdf;

		pdf = list_entry(pdflush_list.next, struct pdflush_work, list);
		list_del_init(&pdf->list);
		if (list_empty(&pdflush_list))
			last_empty_jifs = jiffies;
		pdf->fn = fn;
		pdf->arg0 = arg0;
		wake_up_process(pdf->who);
	}
	spin_unlock_irqrestore(&pdflush_lock, flags);

	return ret;
}

static void start_one_pdflush_thread(void)
{
	kthread_run(pdflush, NULL, "pdflush");
}

static int __init pdflush_init(void)
{
	int i;

	for (i = 0; i < MIN_PDFLUSH_THREADS; i++)
		start_one_pdflush_thread();
	return 0;
}

module_init(pdflush_init);
