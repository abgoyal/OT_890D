

#include "linux/sched.h"
#include "linux/init_task.h"
#include "linux/fs.h"
#include "linux/module.h"
#include "linux/mqueue.h"
#include "asm/uaccess.h"

struct mm_struct init_mm = INIT_MM(init_mm);
static struct signal_struct init_signals = INIT_SIGNALS(init_signals);
static struct sighand_struct init_sighand = INIT_SIGHAND(init_sighand);
EXPORT_SYMBOL(init_mm);


struct task_struct init_task = INIT_TASK(init_task);

EXPORT_SYMBOL(init_task);


union thread_union init_thread_union
	__attribute__((__section__(".data.init_task"))) =
		{ INIT_THREAD_INFO(init_task) };

union thread_union cpu0_irqstack
	__attribute__((__section__(".data.init_irqstack"))) =
		{ INIT_THREAD_INFO(init_task) };
