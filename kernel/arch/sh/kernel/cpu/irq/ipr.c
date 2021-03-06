
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/interrupt.h>

static inline struct ipr_desc *get_ipr_desc(unsigned int irq)
{
	struct irq_chip *chip = get_irq_chip(irq);
	return (void *)((char *)chip - offsetof(struct ipr_desc, chip));
}

static void disable_ipr_irq(unsigned int irq)
{
	struct ipr_data *p = get_irq_chip_data(irq);
	unsigned long addr = get_ipr_desc(irq)->ipr_offsets[p->ipr_idx];
	/* Set the priority in IPR to 0 */
	__raw_writew(__raw_readw(addr) & (0xffff ^ (0xf << p->shift)), addr);
}

static void enable_ipr_irq(unsigned int irq)
{
	struct ipr_data *p = get_irq_chip_data(irq);
	unsigned long addr = get_ipr_desc(irq)->ipr_offsets[p->ipr_idx];
	/* Set priority in IPR back to original value */
	__raw_writew(__raw_readw(addr) | (p->priority << p->shift), addr);
}

void register_ipr_controller(struct ipr_desc *desc)
{
	int i;

	desc->chip.mask = disable_ipr_irq;
	desc->chip.unmask = enable_ipr_irq;
	desc->chip.mask_ack = disable_ipr_irq;

	for (i = 0; i < desc->nr_irqs; i++) {
		struct ipr_data *p = desc->ipr_data + i;

		BUG_ON(p->ipr_idx >= desc->nr_offsets);
		BUG_ON(!desc->ipr_offsets[p->ipr_idx]);

		disable_irq_nosync(p->irq);
		set_irq_chip_and_handler_name(p->irq, &desc->chip,
				      handle_level_irq, "level");
		set_irq_chip_data(p->irq, p);
		disable_ipr_irq(p->irq);
	}
}
EXPORT_SYMBOL(register_ipr_controller);
