
#ifndef __BFIN_HARDIRQ_H
#define __BFIN_HARDIRQ_H

#include <linux/cache.h>
#include <linux/threads.h>
#include <asm/irq.h>

typedef struct {
	unsigned int __softirq_pending;
	unsigned int __syscall_count;
	struct task_struct *__ksoftirqd_task;
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>	/* Standard mappings for irq_cpustat_t above */


#if NR_IRQS > 256
#define HARDIRQ_BITS	9
#else
#define HARDIRQ_BITS	8
#endif

#ifdef NR_IRQS
# if (1 << HARDIRQ_BITS) < NR_IRQS
# error HARDIRQ_BITS is too low!
# endif
#endif

#define __ARCH_IRQ_EXIT_IRQS_DISABLED	1

extern void ack_bad_irq(unsigned int irq);

#endif
