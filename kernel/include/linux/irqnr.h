
#ifndef _LINUX_IRQNR_H
#define _LINUX_IRQNR_H

#include <asm/tcm.h>

#ifdef __KERNEL__

#ifndef CONFIG_GENERIC_HARDIRQS
#include <asm/irq.h>

#define nr_irqs			NR_IRQS
#define irq_to_desc(irq)	(&irq_desc[irq])

# define for_each_irq_desc(irq, desc)		\
	for (irq = 0; irq < nr_irqs; irq++)

# define for_each_irq_desc_reverse(irq, desc)                          \
	for (irq = nr_irqs - 1; irq >= 0; irq--)
#else /* CONFIG_GENERIC_HARDIRQS */

extern int nr_irqs;
extern struct irq_desc * __tcmfunc irq_to_desc(unsigned int irq);

# define for_each_irq_desc(irq, desc)					\
	for (irq = 0, desc = irq_to_desc(irq); irq < nr_irqs;		\
	     irq++, desc = irq_to_desc(irq))				\
		if (desc)


# define for_each_irq_desc_reverse(irq, desc)				\
	for (irq = nr_irqs - 1, desc = irq_to_desc(irq); irq >= 0;	\
	     irq--, desc = irq_to_desc(irq))				\
		if (desc)

#endif /* CONFIG_GENERIC_HARDIRQS */

#define for_each_irq_nr(irq)                   \
       for (irq = 0; irq < nr_irqs; irq++)

#endif /* __KERNEL__ */

#endif
