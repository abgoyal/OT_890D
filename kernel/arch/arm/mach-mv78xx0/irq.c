

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/irq.h>
#include <asm/gpio.h>
#include <mach/mv78xx0.h>
#include <plat/irq.h>
#include "common.h"

static void gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	BUG_ON(irq < IRQ_MV78XX0_GPIO_0_7 || irq > IRQ_MV78XX0_GPIO_24_31);

	orion_gpio_irq_handler((irq - IRQ_MV78XX0_GPIO_0_7) << 3);
}

void __init mv78xx0_init_irq(void)
{
	int i;

	orion_irq_init(0, (void __iomem *)(IRQ_VIRT_BASE + IRQ_MASK_LOW_OFF));
	orion_irq_init(32, (void __iomem *)(IRQ_VIRT_BASE + IRQ_MASK_HIGH_OFF));
	orion_irq_init(64, (void __iomem *)(IRQ_VIRT_BASE + IRQ_MASK_ERR_OFF));

	/*
	 * Mask and clear GPIO IRQ interrupts.
	 */
	writel(0, GPIO_LEVEL_MASK(0));
	writel(0, GPIO_EDGE_MASK(0));
	writel(0, GPIO_EDGE_CAUSE(0));

	for (i = IRQ_MV78XX0_GPIO_START; i < NR_IRQS; i++) {
		set_irq_chip(i, &orion_gpio_irq_chip);
		set_irq_handler(i, handle_level_irq);
		irq_desc[i].status |= IRQ_LEVEL;
		set_irq_flags(i, IRQF_VALID);
	}
	set_irq_chained_handler(IRQ_MV78XX0_GPIO_0_7, gpio_irq_handler);
	set_irq_chained_handler(IRQ_MV78XX0_GPIO_8_15, gpio_irq_handler);
	set_irq_chained_handler(IRQ_MV78XX0_GPIO_16_23, gpio_irq_handler);
	set_irq_chained_handler(IRQ_MV78XX0_GPIO_24_31, gpio_irq_handler);
}
