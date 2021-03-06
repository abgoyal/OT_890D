

#include <linux/linkage.h>

#include <asm/asm-offsets.h>
#include <asm/thread_info.h>
#include <asm/percpu.h>
#include <asm/processor-flags.h>
#include <asm/segment.h>

#include <xen/interface/xen.h>

#define RELOC(x, v)	.globl x##_reloc; x##_reloc=v
#define ENDPATCH(x)	.globl x##_end; x##_end=.

/* Pseudo-flag used for virtual NMI, which we don't implement yet */
#define XEN_EFLAGS_NMI	0x80000000

ENTRY(xen_irq_enable_direct)
	/* Unmask events */
	movb $0, PER_CPU_VAR(xen_vcpu_info)+XEN_vcpu_info_mask

	/* Preempt here doesn't matter because that will deal with
	   any pending interrupts.  The pending check may end up being
	   run on the wrong CPU, but that doesn't hurt. */

	/* Test for pending */
	testb $0xff, PER_CPU_VAR(xen_vcpu_info)+XEN_vcpu_info_pending
	jz 1f

2:	call check_events
1:
ENDPATCH(xen_irq_enable_direct)
	ret
	ENDPROC(xen_irq_enable_direct)
	RELOC(xen_irq_enable_direct, 2b+1)


ENTRY(xen_irq_disable_direct)
	movb $1, PER_CPU_VAR(xen_vcpu_info)+XEN_vcpu_info_mask
ENDPATCH(xen_irq_disable_direct)
	ret
	ENDPROC(xen_irq_disable_direct)
	RELOC(xen_irq_disable_direct, 0)

ENTRY(xen_save_fl_direct)
	testb $0xff, PER_CPU_VAR(xen_vcpu_info)+XEN_vcpu_info_mask
	setz %ah
	addb %ah,%ah
ENDPATCH(xen_save_fl_direct)
	ret
	ENDPROC(xen_save_fl_direct)
	RELOC(xen_save_fl_direct, 0)


ENTRY(xen_restore_fl_direct)
	testb $X86_EFLAGS_IF>>8, %ah
	setz PER_CPU_VAR(xen_vcpu_info)+XEN_vcpu_info_mask
	/* Preempt here doesn't matter because that will deal with
	   any pending interrupts.  The pending check may end up being
	   run on the wrong CPU, but that doesn't hurt. */

	/* check for unmasked and pending */
	cmpw $0x0001, PER_CPU_VAR(xen_vcpu_info)+XEN_vcpu_info_pending
	jz 1f
2:	call check_events
1:
ENDPATCH(xen_restore_fl_direct)
	ret
	ENDPROC(xen_restore_fl_direct)
	RELOC(xen_restore_fl_direct, 2b+1)

ENTRY(xen_sysexit)
	movl PT_EAX(%esp), %eax			/* Shouldn't be necessary? */
	orl $X86_EFLAGS_IF, PT_EFLAGS(%esp)
	lea PT_EIP(%esp), %esp

	jmp xen_iret
ENDPROC(xen_sysexit)

ENTRY(xen_iret)
	/* test eflags for special cases */
	testl $(X86_EFLAGS_VM | XEN_EFLAGS_NMI), 8(%esp)
	jnz hyper_iret

	push %eax
	ESP_OFFSET=4	# bytes pushed onto stack

	/* Store vcpu_info pointer for easy access.  Do it this
	   way to avoid having to reload %fs */
#ifdef CONFIG_SMP
	GET_THREAD_INFO(%eax)
	movl TI_cpu(%eax),%eax
	movl __per_cpu_offset(,%eax,4),%eax
	mov per_cpu__xen_vcpu(%eax),%eax
#else
	movl per_cpu__xen_vcpu, %eax
#endif

	/* check IF state we're restoring */
	testb $X86_EFLAGS_IF>>8, 8+1+ESP_OFFSET(%esp)

	/* Maybe enable events.  Once this happens we could get a
	   recursive event, so the critical region starts immediately
	   afterwards.  However, if that happens we don't end up
	   resuming the code, so we don't have to be worried about
	   being preempted to another CPU. */
	setz XEN_vcpu_info_mask(%eax)
xen_iret_start_crit:

	/* check for unmasked and pending */
	cmpw $0x0001, XEN_vcpu_info_pending(%eax)

	/* If there's something pending, mask events again so we
	   can jump back into xen_hypervisor_callback */
	sete XEN_vcpu_info_mask(%eax)

	popl %eax

	/* From this point on the registers are restored and the stack
	   updated, so we don't need to worry about it if we're preempted */
iret_restore_end:

	/* Jump to hypervisor_callback after fixing up the stack.
	   Events are masked, so jumping out of the critical
	   region is OK. */
	je xen_hypervisor_callback

1:	iret
xen_iret_end_crit:
.section __ex_table,"a"
	.align 4
	.long 1b,iret_exc
.previous

hyper_iret:
	/* put this out of line since its very rarely used */
	jmp hypercall_page + __HYPERVISOR_iret * 32

	.globl xen_iret_start_crit, xen_iret_end_crit

ENTRY(xen_iret_crit_fixup)
	/*
	   Paranoia: Make sure we're really coming from kernel space.
	   One could imagine a case where userspace jumps into the
	   critical range address, but just before the CPU delivers a GP,
	   it decides to deliver an interrupt instead.  Unlikely?
	   Definitely.  Easy to avoid?  Yes.  The Intel documents
	   explicitly say that the reported EIP for a bad jump is the
	   jump instruction itself, not the destination, but some virtual
	   environments get this wrong.
	 */
	movl PT_CS(%esp), %ecx
	andl $SEGMENT_RPL_MASK, %ecx
	cmpl $USER_RPL, %ecx
	je 2f

	lea PT_ORIG_EAX(%esp), %esi
	lea PT_EFLAGS(%esp), %edi

	/* If eip is before iret_restore_end then stack
	   hasn't been restored yet. */
	cmp $iret_restore_end, %eax
	jae 1f

	movl 0+4(%edi),%eax		/* copy EAX (just above top of frame) */
	movl %eax, PT_EAX(%esp)

	lea ESP_OFFSET(%edi),%edi	/* move dest up over saved regs */

	/* set up the copy */
1:	std
	mov $PT_EIP / 4, %ecx		/* saved regs up to orig_eax */
	rep movsl
	cld

	lea 4(%edi),%esp		/* point esp to new frame */
2:	jmp xen_do_upcall


check_events:
	push %eax
	push %ecx
	push %edx
	call xen_force_evtchn_callback
	pop %edx
	pop %ecx
	pop %eax
	ret
