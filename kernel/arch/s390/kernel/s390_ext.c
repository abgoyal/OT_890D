

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/kernel_stat.h>
#include <linux/interrupt.h>
#include <asm/cpu.h>
#include <asm/lowcore.h>
#include <asm/s390_ext.h>
#include <asm/irq_regs.h>
#include <asm/irq.h>
#include "entry.h"

ext_int_info_t *ext_int_hash[256] = { NULL, };

static inline int ext_hash(__u16 code)
{
	return (code + (code >> 9)) & 0xff;
}

int register_external_interrupt(__u16 code, ext_int_handler_t handler)
{
        ext_int_info_t *p;
        int index;

	p = kmalloc(sizeof(ext_int_info_t), GFP_ATOMIC);
        if (p == NULL)
                return -ENOMEM;
        p->code = code;
        p->handler = handler;
	index = ext_hash(code);
        p->next = ext_int_hash[index];
        ext_int_hash[index] = p;
        return 0;
}

int register_early_external_interrupt(__u16 code, ext_int_handler_t handler,
				      ext_int_info_t *p)
{
        int index;

        if (p == NULL)
                return -EINVAL;
        p->code = code;
        p->handler = handler;
	index = ext_hash(code);
        p->next = ext_int_hash[index];
        ext_int_hash[index] = p;
        return 0;
}

int unregister_external_interrupt(__u16 code, ext_int_handler_t handler)
{
        ext_int_info_t *p, *q;
        int index;

	index = ext_hash(code);
        q = NULL;
        p = ext_int_hash[index];
        while (p != NULL) {
                if (p->code == code && p->handler == handler)
                        break;
                q = p;
                p = p->next;
        }
        if (p == NULL)
                return -ENOENT;
        if (q != NULL)
                q->next = p->next;
        else
                ext_int_hash[index] = p->next;
	kfree(p);
        return 0;
}

int unregister_early_external_interrupt(__u16 code, ext_int_handler_t handler,
					ext_int_info_t *p)
{
	ext_int_info_t *q;
	int index;

	if (p == NULL || p->code != code || p->handler != handler)
		return -EINVAL;
	index = ext_hash(code);
	q = ext_int_hash[index];
	if (p != q) {
		while (q != NULL) {
			if (q->next == p)
				break;
			q = q->next;
		}
		if (q == NULL)
			return -ENOENT;
		q->next = p->next;
	} else
		ext_int_hash[index] = p->next;
	return 0;
}

void do_extint(struct pt_regs *regs, unsigned short code)
{
        ext_int_info_t *p;
        int index;
	struct pt_regs *old_regs;

	old_regs = set_irq_regs(regs);
	s390_idle_check();
	irq_enter();
	if (S390_lowcore.int_clock >= S390_lowcore.clock_comparator)
		/* Serve timer interrupts first. */
		clock_comparator_work();
	kstat_cpu(smp_processor_id()).irqs[EXTERNAL_INTERRUPT]++;
        index = ext_hash(code);
	for (p = ext_int_hash[index]; p; p = p->next) {
		if (likely(p->code == code))
			p->handler(code);
	}
	irq_exit();
	set_irq_regs(old_regs);
}

EXPORT_SYMBOL(register_external_interrupt);
EXPORT_SYMBOL(unregister_external_interrupt);
