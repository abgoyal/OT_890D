
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/kernel_stat.h>
#include <linux/mm.h>
#include <linux/cache.h>
#include <linux/delay.h>
#include <linux/efi.h>
#include <linux/bitops.h>
#include <linux/kexec.h>

#include <asm/atomic.h>
#include <asm/current.h>
#include <asm/delay.h>
#include <asm/machvec.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/sal.h>
#include <asm/system.h>
#include <asm/tlbflush.h>
#include <asm/unistd.h>
#include <asm/mca.h>

static struct local_tlb_flush_counts {
	unsigned int count;
} __attribute__((__aligned__(32))) local_tlb_flush_counts[NR_CPUS];

static DEFINE_PER_CPU(unsigned short, shadow_flush_counts[NR_CPUS]) ____cacheline_aligned;

#define IPI_CALL_FUNC		0
#define IPI_CPU_STOP		1
#define IPI_CALL_FUNC_SINGLE	2
#define IPI_KDUMP_CPU_STOP	3

/* This needs to be cacheline aligned because it is written to by *other* CPUs.  */
static DEFINE_PER_CPU_SHARED_ALIGNED(u64, ipi_operation);

extern void cpu_halt (void);

static void
stop_this_cpu(void)
{
	/*
	 * Remove this CPU:
	 */
	cpu_clear(smp_processor_id(), cpu_online_map);
	max_xtp();
	local_irq_disable();
	cpu_halt();
}

void
cpu_die(void)
{
	max_xtp();
	local_irq_disable();
	cpu_halt();
	/* Should never be here */
	BUG();
	for (;;);
}

irqreturn_t
handle_IPI (int irq, void *dev_id)
{
	int this_cpu = get_cpu();
	unsigned long *pending_ipis = &__ia64_per_cpu_var(ipi_operation);
	unsigned long ops;

	mb();	/* Order interrupt and bit testing. */
	while ((ops = xchg(pending_ipis, 0)) != 0) {
		mb();	/* Order bit clearing and data access. */
		do {
			unsigned long which;

			which = ffz(~ops);
			ops &= ~(1 << which);

			switch (which) {
			case IPI_CPU_STOP:
				stop_this_cpu();
				break;
			case IPI_CALL_FUNC:
				generic_smp_call_function_interrupt();
				break;
			case IPI_CALL_FUNC_SINGLE:
				generic_smp_call_function_single_interrupt();
				break;
#ifdef CONFIG_KEXEC
			case IPI_KDUMP_CPU_STOP:
				unw_init_running(kdump_cpu_freeze, NULL);
				break;
#endif
			default:
				printk(KERN_CRIT "Unknown IPI on CPU %d: %lu\n",
						this_cpu, which);
				break;
			}
		} while (ops);
		mb();	/* Order data access and bit testing. */
	}
	put_cpu();
	return IRQ_HANDLED;
}



static inline void
send_IPI_single (int dest_cpu, int op)
{
	set_bit(op, &per_cpu(ipi_operation, dest_cpu));
	platform_send_ipi(dest_cpu, IA64_IPI_VECTOR, IA64_IPI_DM_INT, 0);
}

static inline void
send_IPI_allbutself (int op)
{
	unsigned int i;

	for_each_online_cpu(i) {
		if (i != smp_processor_id())
			send_IPI_single(i, op);
	}
}

static inline void
send_IPI_mask(cpumask_t mask, int op)
{
	unsigned int cpu;

	for_each_cpu_mask(cpu, mask) {
			send_IPI_single(cpu, op);
	}
}

static inline void
send_IPI_all (int op)
{
	int i;

	for_each_online_cpu(i) {
		send_IPI_single(i, op);
	}
}

static inline void
send_IPI_self (int op)
{
	send_IPI_single(smp_processor_id(), op);
}

#ifdef CONFIG_KEXEC
void
kdump_smp_send_stop(void)
{
 	send_IPI_allbutself(IPI_KDUMP_CPU_STOP);
}

void
kdump_smp_send_init(void)
{
	unsigned int cpu, self_cpu;
	self_cpu = smp_processor_id();
	for_each_online_cpu(cpu) {
		if (cpu != self_cpu) {
			if(kdump_status[cpu] == 0)
				platform_send_ipi(cpu, 0, IA64_IPI_DM_INIT, 0);
		}
	}
}
#endif
void
smp_send_reschedule (int cpu)
{
	platform_send_ipi(cpu, IA64_IPI_RESCHEDULE, IA64_IPI_DM_INT, 0);
}

static void
smp_send_local_flush_tlb (int cpu)
{
	platform_send_ipi(cpu, IA64_IPI_LOCAL_TLB_FLUSH, IA64_IPI_DM_INT, 0);
}

void
smp_local_flush_tlb(void)
{
	/*
	 * Use atomic ops. Otherwise, the load/increment/store sequence from
	 * a "++" operation can have the line stolen between the load & store.
	 * The overhead of the atomic op in negligible in this case & offers
	 * significant benefit for the brief periods where lots of cpus
	 * are simultaneously flushing TLBs.
	 */
	ia64_fetchadd(1, &local_tlb_flush_counts[smp_processor_id()].count, acq);
	local_flush_tlb_all();
}

#define FLUSH_DELAY	5 /* Usec backoff to eliminate excessive cacheline bouncing */

void
smp_flush_tlb_cpumask(cpumask_t xcpumask)
{
	unsigned short *counts = __ia64_per_cpu_var(shadow_flush_counts);
	cpumask_t cpumask = xcpumask;
	int mycpu, cpu, flush_mycpu = 0;

	preempt_disable();
	mycpu = smp_processor_id();

	for_each_cpu_mask(cpu, cpumask)
		counts[cpu] = local_tlb_flush_counts[cpu].count & 0xffff;

	mb();
	for_each_cpu_mask(cpu, cpumask) {
		if (cpu == mycpu)
			flush_mycpu = 1;
		else
			smp_send_local_flush_tlb(cpu);
	}

	if (flush_mycpu)
		smp_local_flush_tlb();

	for_each_cpu_mask(cpu, cpumask)
		while(counts[cpu] == (local_tlb_flush_counts[cpu].count & 0xffff))
			udelay(FLUSH_DELAY);

	preempt_enable();
}

void
smp_flush_tlb_all (void)
{
	on_each_cpu((void (*)(void *))local_flush_tlb_all, NULL, 1);
}

void
smp_flush_tlb_mm (struct mm_struct *mm)
{
	preempt_disable();
	/* this happens for the common case of a single-threaded fork():  */
	if (likely(mm == current->active_mm && atomic_read(&mm->mm_users) == 1))
	{
		local_finish_flush_tlb_mm(mm);
		preempt_enable();
		return;
	}

	preempt_enable();
	/*
	 * We could optimize this further by using mm->cpu_vm_mask to track which CPUs
	 * have been running in the address space.  It's not clear that this is worth the
	 * trouble though: to avoid races, we have to raise the IPI on the target CPU
	 * anyhow, and once a CPU is interrupted, the cost of local_flush_tlb_all() is
	 * rather trivial.
	 */
	on_each_cpu((void (*)(void *))local_finish_flush_tlb_mm, mm, 1);
}

void arch_send_call_function_single_ipi(int cpu)
{
	send_IPI_single(cpu, IPI_CALL_FUNC_SINGLE);
}

void arch_send_call_function_ipi(cpumask_t mask)
{
	send_IPI_mask(mask, IPI_CALL_FUNC);
}

void
smp_send_stop (void)
{
	send_IPI_allbutself(IPI_CPU_STOP);
}

int
setup_profiling_timer (unsigned int multiplier)
{
	return -EINVAL;
}