

#include <linux/kernel.h>
#include <linux/jffs2.h>
#include <linux/mtd/mtd.h>
#include <linux/completion.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include "nodelist.h"


static int jffs2_garbage_collect_thread(void *);

void jffs2_garbage_collect_trigger(struct jffs2_sb_info *c)
{
	spin_lock(&c->erase_completion_lock);
	if (c->gc_task && jffs2_thread_should_wake(c))
		send_sig(SIGHUP, c->gc_task, 1);
	spin_unlock(&c->erase_completion_lock);
}

/* This must only ever be called when no GC thread is currently running */
int jffs2_start_garbage_collect_thread(struct jffs2_sb_info *c)
{
	pid_t pid;
	int ret = 0;

	BUG_ON(c->gc_task);

	init_completion(&c->gc_thread_start);
	init_completion(&c->gc_thread_exit);

	pid = kernel_thread(jffs2_garbage_collect_thread, c, CLONE_FS|CLONE_FILES);
	if (pid < 0) {
		printk(KERN_WARNING "fork failed for JFFS2 garbage collect thread: %d\n", -pid);
		complete(&c->gc_thread_exit);
		ret = pid;
	} else {
		/* Wait for it... */
		D1(printk(KERN_DEBUG "JFFS2: Garbage collect thread is pid %d\n", pid));
		wait_for_completion(&c->gc_thread_start);
	}

	return ret;
}

void jffs2_stop_garbage_collect_thread(struct jffs2_sb_info *c)
{
	int wait = 0;
	spin_lock(&c->erase_completion_lock);
	if (c->gc_task) {
		D1(printk(KERN_DEBUG "jffs2: Killing GC task %d\n", c->gc_task->pid));
		send_sig(SIGKILL, c->gc_task, 1);
		wait = 1;
	}
	spin_unlock(&c->erase_completion_lock);
	if (wait)
		wait_for_completion(&c->gc_thread_exit);
}

static int jffs2_garbage_collect_thread(void *_c)
{
	struct jffs2_sb_info *c = _c;

	daemonize("jffs2_gcd_mtd%d", c->mtd->index);
	allow_signal(SIGKILL);
	allow_signal(SIGSTOP);
	allow_signal(SIGCONT);

	c->gc_task = current;
	complete(&c->gc_thread_start);

	set_user_nice(current, 10);

	set_freezable();
	for (;;) {
		allow_signal(SIGHUP);
	again:
		spin_lock(&c->erase_completion_lock);
		if (!jffs2_thread_should_wake(c)) {
			set_current_state (TASK_INTERRUPTIBLE);
			spin_unlock(&c->erase_completion_lock);
			D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread sleeping...\n"));
			schedule();
		} else
			spin_unlock(&c->erase_completion_lock);
			

		/* Problem - immediately after bootup, the GCD spends a lot
		 * of time in places like jffs2_kill_fragtree(); so much so
		 * that userspace processes (like gdm and X) are starved
		 * despite plenty of cond_resched()s and renicing.  Yield()
		 * doesn't help, either (presumably because userspace and GCD
		 * are generally competing for a higher latency resource -
		 * disk).
		 * This forces the GCD to slow the hell down.   Pulling an
		 * inode in with read_inode() is much preferable to having
		 * the GC thread get there first. */
		schedule_timeout_interruptible(msecs_to_jiffies(50));

		/* Put_super will send a SIGKILL and then wait on the sem.
		 */
		while (signal_pending(current) || freezing(current)) {
			siginfo_t info;
			unsigned long signr;

			if (try_to_freeze())
				goto again;

			signr = dequeue_signal_lock(current, &current->blocked, &info);

			switch(signr) {
			case SIGSTOP:
				D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): SIGSTOP received.\n"));
				set_current_state(TASK_STOPPED);
				schedule();
				break;

			case SIGKILL:
				D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): SIGKILL received.\n"));
				goto die;

			case SIGHUP:
				D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): SIGHUP received.\n"));
				break;
			default:
				D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): signal %ld received\n", signr));
			}
		}
		/* We don't want SIGHUP to interrupt us. STOP and KILL are OK though. */
		disallow_signal(SIGHUP);

		D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): pass\n"));
		if (jffs2_garbage_collect_pass(c) == -ENOSPC) {
			printk(KERN_NOTICE "No space for garbage collection. Aborting GC thread\n");
			goto die;
		}
	}
 die:
	spin_lock(&c->erase_completion_lock);
	c->gc_task = NULL;
	spin_unlock(&c->erase_completion_lock);
	complete_and_exit(&c->gc_thread_exit, 0);
}
