

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sysdev.h>

void ack_bad_irq(unsigned int irq)
{
	printk("unexpected IRQ %u\n", irq);
}

/* May be overridden by platform code */
int __weak nmi_enable(void)
{
	return -ENOSYS;
}

void __weak nmi_disable(void)
{

}

#ifdef CONFIG_PROC_FS
int show_interrupts(struct seq_file *p, void *v)
{
	int i = *(loff_t *)v, cpu;
	struct irqaction *action;
	unsigned long flags;

	if (i == 0) {
		seq_puts(p, "           ");
		for_each_online_cpu(cpu)
			seq_printf(p, "CPU%d       ", cpu);
		seq_putc(p, '\n');
	}

	if (i < NR_IRQS) {
		spin_lock_irqsave(&irq_desc[i].lock, flags);
		action = irq_desc[i].action;
		if (!action)
			goto unlock;

		seq_printf(p, "%3d: ", i);
		for_each_online_cpu(cpu)
			seq_printf(p, "%10u ", kstat_cpu(cpu).irqs[i]);
		seq_printf(p, " %8s", irq_desc[i].chip->name ? : "-");
		seq_printf(p, "  %s", action->name);
		for (action = action->next; action; action = action->next)
			seq_printf(p, ", %s", action->name);

		seq_putc(p, '\n');
	unlock:
		spin_unlock_irqrestore(&irq_desc[i].lock, flags);
	}

	return 0;
}
#endif
