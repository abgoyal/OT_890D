

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/signal.h>
#include <linux/uaccess.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/processor.h>
#include <asm/asm-offsets.h>
#include <asm/dma.h>
#include <asm/fixed_code.h>
#include <asm/cacheflush.h>
#include <asm/mem_map.h>

#define TEXT_OFFSET 0

/* determines which bits in the SYSCFG reg the user has access to. */
/* 1 = access 0 = no access */
#define SYSCFG_MASK 0x0007	/* SYSCFG reg */
/* sets the trace bits. */
#define TRACE_BITS 0x0001

/* Find the stack offset for a register, relative to thread.esp0. */
#define PT_REG(reg)	((long)&((struct pt_regs *)0)->reg)

static inline struct pt_regs *get_user_regs(struct task_struct *task)
{
	return (struct pt_regs *)
	    ((unsigned long)task_stack_page(task) +
	     (THREAD_SIZE - sizeof(struct pt_regs)));
}

static inline int ptrace_getregs(struct task_struct *tsk, void __user *uregs)
{
	struct pt_regs regs;
	memcpy(&regs, get_user_regs(tsk), sizeof(regs));
	regs.usp = tsk->thread.usp;
	return copy_to_user(uregs, &regs, sizeof(struct pt_regs)) ? -EFAULT : 0;
}


static inline long get_reg(struct task_struct *task, int regno)
{
	unsigned char *reg_ptr;

	struct pt_regs *regs =
	    (struct pt_regs *)((unsigned long)task_stack_page(task) +
			       (THREAD_SIZE - sizeof(struct pt_regs)));
	reg_ptr = (char *)regs;

	switch (regno) {
	case PT_USP:
		return task->thread.usp;
	default:
		if (regno <= 216)
			return *(long *)(reg_ptr + regno);
	}
	/* slight mystery ... never seems to come here but kernel misbehaves without this code! */

	printk(KERN_WARNING "Request to get for unknown register %d\n", regno);
	return 0;
}

static inline int
put_reg(struct task_struct *task, int regno, unsigned long data)
{
	char *reg_ptr;

	struct pt_regs *regs =
	    (struct pt_regs *)((unsigned long)task_stack_page(task) +
			       (THREAD_SIZE - sizeof(struct pt_regs)));
	reg_ptr = (char *)regs;

	switch (regno) {
	case PT_PC:
		/*********************************************************************/
		/* At this point the kernel is most likely in exception.             */
		/* The RETX register will be used to populate the pc of the process. */
		/*********************************************************************/
		regs->retx = data;
		regs->pc = data;
		break;
	case PT_RETX:
		break;		/* regs->retx = data; break; */
	case PT_USP:
		regs->usp = data;
		task->thread.usp = data;
		break;
	default:
		if (regno <= 216)
			*(long *)(reg_ptr + regno) = data;
	}
	return 0;
}

static inline int is_user_addr_valid(struct task_struct *child,
				     unsigned long start, unsigned long len)
{
	struct vm_area_struct *vma;
	struct sram_list_struct *sraml;

	/* overflow */
	if (start + len < start)
		return -EIO;

	vma = find_vma(child->mm, start);
	if (vma && start >= vma->vm_start && start + len <= vma->vm_end)
			return 0;

	for (sraml = child->mm->context.sram_list; sraml; sraml = sraml->next)
		if (start >= (unsigned long)sraml->addr
		    && start + len < (unsigned long)sraml->addr + sraml->length)
			return 0;

	if (start >= FIXED_CODE_START && start + len < FIXED_CODE_END)
		return 0;

	return -EIO;
}

void ptrace_enable(struct task_struct *child)
{
	unsigned long tmp;
	tmp = get_reg(child, PT_SYSCFG) | (TRACE_BITS);
	put_reg(child, PT_SYSCFG, tmp);
}

void ptrace_disable(struct task_struct *child)
{
	unsigned long tmp;
	/* make sure the single step bit is not set. */
	tmp = get_reg(child, PT_SYSCFG) & ~TRACE_BITS;
	put_reg(child, PT_SYSCFG, tmp);
}

long arch_ptrace(struct task_struct *child, long request, long addr, long data)
{
	int ret;
	unsigned long __user *datap = (unsigned long __user *)data;

	switch (request) {
		/* when I and D space are separate, these will need to be fixed. */
	case PTRACE_PEEKDATA:
		pr_debug("ptrace: PEEKDATA\n");
		/* fall through */
	case PTRACE_PEEKTEXT:	/* read word at location addr. */
		{
			unsigned long tmp = 0;
			int copied;

			ret = -EIO;
			pr_debug("ptrace: PEEKTEXT at addr 0x%08lx + %ld\n", addr, sizeof(data));
			if (is_user_addr_valid(child, addr, sizeof(tmp)) < 0)
				break;
			pr_debug("ptrace: user address is valid\n");

			if (L1_CODE_LENGTH != 0 && addr >= get_l1_code_start()
			    && addr + sizeof(tmp) <= get_l1_code_start() + L1_CODE_LENGTH) {
				safe_dma_memcpy (&tmp, (const void *)(addr), sizeof(tmp));
				copied = sizeof(tmp);

			} else if (L1_DATA_A_LENGTH != 0 && addr >= L1_DATA_A_START
			    && addr + sizeof(tmp) <= L1_DATA_A_START + L1_DATA_A_LENGTH) {
				memcpy(&tmp, (const void *)(addr), sizeof(tmp));
				copied = sizeof(tmp);

			} else if (L1_DATA_B_LENGTH != 0 && addr >= L1_DATA_B_START
			    && addr + sizeof(tmp) <= L1_DATA_B_START + L1_DATA_B_LENGTH) {
				memcpy(&tmp, (const void *)(addr), sizeof(tmp));
				copied = sizeof(tmp);

			} else if (addr >= FIXED_CODE_START
			    && addr + sizeof(tmp) <= FIXED_CODE_END) {
				copy_from_user_page(0, 0, 0, &tmp, (const void *)(addr), sizeof(tmp));
				copied = sizeof(tmp);

			} else
				copied = access_process_vm(child, addr, &tmp,
							   sizeof(tmp), 0);

			pr_debug("ptrace: copied size %d [0x%08lx]\n", copied, tmp);
			if (copied != sizeof(tmp))
				break;
			ret = put_user(tmp, datap);
			break;
		}

		/* read the word at location addr in the USER area. */
	case PTRACE_PEEKUSR:
		{
			unsigned long tmp;
			ret = -EIO;
			tmp = 0;
			if ((addr & 3) || (addr > (sizeof(struct pt_regs) + 16))) {
				printk(KERN_WARNING "ptrace error : PEEKUSR : temporarily returning "
				                    "0 - %x sizeof(pt_regs) is %lx\n",
				     (int)addr, sizeof(struct pt_regs));
				break;
			}
			if (addr == sizeof(struct pt_regs)) {
				/* PT_TEXT_ADDR */
				tmp = child->mm->start_code + TEXT_OFFSET;
			} else if (addr == (sizeof(struct pt_regs) + 4)) {
				/* PT_TEXT_END_ADDR */
				tmp = child->mm->end_code;
			} else if (addr == (sizeof(struct pt_regs) + 8)) {
				/* PT_DATA_ADDR */
				tmp = child->mm->start_data;
#ifdef CONFIG_BINFMT_ELF_FDPIC
			} else if (addr == (sizeof(struct pt_regs) + 12)) {
				tmp = child->mm->context.exec_fdpic_loadmap;
			} else if (addr == (sizeof(struct pt_regs) + 16)) {
				tmp = child->mm->context.interp_fdpic_loadmap;
#endif
			} else {
				tmp = get_reg(child, addr);
			}
			ret = put_user(tmp, datap);
			break;
		}

		/* when I and D space are separate, this will have to be fixed. */
	case PTRACE_POKEDATA:
		pr_debug("ptrace: PTRACE_PEEKDATA\n");
		/* fall through */
	case PTRACE_POKETEXT:	/* write the word at location addr. */
		{
			int copied;

			ret = -EIO;
			pr_debug("ptrace: POKETEXT at addr 0x%08lx + %ld bytes %lx\n",
			         addr, sizeof(data), data);
			if (is_user_addr_valid(child, addr, sizeof(data)) < 0)
				break;
			pr_debug("ptrace: user address is valid\n");

			if (L1_CODE_LENGTH != 0 && addr >= get_l1_code_start()
			    && addr + sizeof(data) <= get_l1_code_start() + L1_CODE_LENGTH) {
				safe_dma_memcpy ((void *)(addr), &data, sizeof(data));
				copied = sizeof(data);

			} else if (L1_DATA_A_LENGTH != 0 && addr >= L1_DATA_A_START
			    && addr + sizeof(data) <= L1_DATA_A_START + L1_DATA_A_LENGTH) {
				memcpy((void *)(addr), &data, sizeof(data));
				copied = sizeof(data);

			} else if (L1_DATA_B_LENGTH != 0 && addr >= L1_DATA_B_START
			    && addr + sizeof(data) <= L1_DATA_B_START + L1_DATA_B_LENGTH) {
				memcpy((void *)(addr), &data, sizeof(data));
				copied = sizeof(data);

			} else if (addr >= FIXED_CODE_START
			    && addr + sizeof(data) <= FIXED_CODE_END) {
				copy_to_user_page(0, 0, 0, (void *)(addr), &data, sizeof(data));
				copied = sizeof(data);

			} else
				copied = access_process_vm(child, addr, &data,
							   sizeof(data), 1);

			pr_debug("ptrace: copied size %d\n", copied);
			if (copied != sizeof(data))
				break;
			ret = 0;
			break;
		}

	case PTRACE_POKEUSR:	/* write the word at location addr in the USER area */
		ret = -EIO;
		if ((addr & 3) || (addr > (sizeof(struct pt_regs) + 16))) {
			printk(KERN_WARNING "ptrace error : POKEUSR: temporarily returning 0\n");
			break;
		}

		if (addr >= (sizeof(struct pt_regs))) {
			ret = 0;
			break;
		}
		if (addr == PT_SYSCFG) {
			data &= SYSCFG_MASK;
			data |= get_reg(child, PT_SYSCFG);
		}
		ret = put_reg(child, addr, data);
		break;

	case PTRACE_SYSCALL:	/* continue and stop at next (return from) syscall */
	case PTRACE_CONT:	/* restart after signal. */
		pr_debug("ptrace: syscall/cont\n");

		ret = -EIO;
		if (!valid_signal(data))
			break;
		if (request == PTRACE_SYSCALL)
			set_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
		else
			clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
		child->exit_code = data;
		ptrace_disable(child);
		pr_debug("ptrace: before wake_up_process\n");
		wake_up_process(child);
		ret = 0;
		break;

	/*
	 * make the child exit.  Best I can do is send it a sigkill.
	 * perhaps it should be put in the status that it wants to
	 * exit.
	 */
	case PTRACE_KILL:
		ret = 0;
		if (child->exit_state == EXIT_ZOMBIE)	/* already dead */
			break;
		child->exit_code = SIGKILL;
		ptrace_disable(child);
		wake_up_process(child);
		break;

	case PTRACE_SINGLESTEP:	/* set the trap flag. */
		pr_debug("ptrace: single step\n");
		ret = -EIO;
		if (!valid_signal(data))
			break;
		clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
		ptrace_enable(child);
		child->exit_code = data;
		wake_up_process(child);
		ret = 0;
		break;

	case PTRACE_GETREGS:
		/* Get all gp regs from the child. */
		ret = ptrace_getregs(child, datap);
		break;

	case PTRACE_SETREGS:
		printk(KERN_WARNING "ptrace: SETREGS: **** NOT IMPLEMENTED ***\n");
		/* Set all gp regs in the child. */
		ret = 0;
		break;

	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}

	return ret;
}

asmlinkage void syscall_trace(void)
{
	if (!test_thread_flag(TIF_SYSCALL_TRACE))
		return;

	if (!(current->ptrace & PT_PTRACED))
		return;

	/* the 0x80 provides a way for the tracing parent to distinguish
	 * between a syscall stop and SIGTRAP delivery
	 */
	ptrace_notify(SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD)
				 ? 0x80 : 0));

	/*
	 * this isn't the same as continuing with a signal, but it will do
	 * for normal use.  strace only continues with a signal if the
	 * stopping signal is not SIGTRAP.  -brl
	 */
	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
}
