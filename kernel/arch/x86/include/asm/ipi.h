
#ifndef _ASM_X86_IPI_H
#define _ASM_X86_IPI_H


#include <asm/hw_irq.h>
#include <asm/apic.h>
#include <asm/smp.h>


static inline unsigned int __prepare_ICR(unsigned int shortcut, int vector,
					 unsigned int dest)
{
	unsigned int icr = shortcut | dest;

	switch (vector) {
	default:
		icr |= APIC_DM_FIXED | vector;
		break;
	case NMI_VECTOR:
		icr |= APIC_DM_NMI;
		break;
	}
	return icr;
}

static inline int __prepare_ICR2(unsigned int mask)
{
	return SET_APIC_DEST_FIELD(mask);
}

static inline void __xapic_wait_icr_idle(void)
{
	while (native_apic_mem_read(APIC_ICR) & APIC_ICR_BUSY)
		cpu_relax();
}

static inline void __send_IPI_shortcut(unsigned int shortcut, int vector,
				       unsigned int dest)
{
	/*
	 * Subtle. In the case of the 'never do double writes' workaround
	 * we have to lock out interrupts to be safe.  As we don't care
	 * of the value read we use an atomic rmw access to avoid costly
	 * cli/sti.  Otherwise we use an even cheaper single atomic write
	 * to the APIC.
	 */
	unsigned int cfg;

	/*
	 * Wait for idle.
	 */
	__xapic_wait_icr_idle();

	/*
	 * No need to touch the target chip field
	 */
	cfg = __prepare_ICR(shortcut, vector, dest);

	/*
	 * Send the IPI. The write to APIC_ICR fires this off.
	 */
	native_apic_mem_write(APIC_ICR, cfg);
}

static inline void __send_IPI_dest_field(unsigned int mask, int vector,
					 unsigned int dest)
{
	unsigned long cfg;

	/*
	 * Wait for idle.
	 */
	if (unlikely(vector == NMI_VECTOR))
		safe_apic_wait_icr_idle();
	else
		__xapic_wait_icr_idle();

	/*
	 * prepare target chip field
	 */
	cfg = __prepare_ICR2(mask);
	native_apic_mem_write(APIC_ICR2, cfg);

	/*
	 * program the ICR
	 */
	cfg = __prepare_ICR(0, vector, dest);

	/*
	 * Send the IPI. The write to APIC_ICR fires this off.
	 */
	native_apic_mem_write(APIC_ICR, cfg);
}

static inline void send_IPI_mask_sequence(const struct cpumask *mask,
					  int vector)
{
	unsigned long flags;
	unsigned long query_cpu;

	/*
	 * Hack. The clustered APIC addressing mode doesn't allow us to send
	 * to an arbitrary mask, so I do a unicast to each CPU instead.
	 * - mbligh
	 */
	local_irq_save(flags);
	for_each_cpu(query_cpu, mask) {
		__send_IPI_dest_field(per_cpu(x86_cpu_to_apicid, query_cpu),
				      vector, APIC_DEST_PHYSICAL);
	}
	local_irq_restore(flags);
}

static inline void send_IPI_mask_allbutself(const struct cpumask *mask,
					    int vector)
{
	unsigned long flags;
	unsigned int query_cpu;
	unsigned int this_cpu = smp_processor_id();

	/* See Hack comment above */

	local_irq_save(flags);
	for_each_cpu(query_cpu, mask)
		if (query_cpu != this_cpu)
			__send_IPI_dest_field(
				per_cpu(x86_cpu_to_apicid, query_cpu),
				vector, APIC_DEST_PHYSICAL);
	local_irq_restore(flags);
}

#endif /* _ASM_X86_IPI_H */
