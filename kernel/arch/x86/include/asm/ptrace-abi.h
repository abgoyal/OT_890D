
#ifndef _ASM_X86_PTRACE_ABI_H
#define _ASM_X86_PTRACE_ABI_H

#ifdef __i386__

#define EBX 0
#define ECX 1
#define EDX 2
#define ESI 3
#define EDI 4
#define EBP 5
#define EAX 6
#define DS 7
#define ES 8
#define FS 9
#define GS 10
#define ORIG_EAX 11
#define EIP 12
#define CS  13
#define EFL 14
#define UESP 15
#define SS   16
#define FRAME_SIZE 17

#else /* __i386__ */

#if defined(__ASSEMBLY__) || defined(__FRAME_OFFSETS)
#define R15 0
#define R14 8
#define R13 16
#define R12 24
#define RBP 32
#define RBX 40
/* arguments: interrupts/non tracing syscalls only save upto here*/
#define R11 48
#define R10 56
#define R9 64
#define R8 72
#define RAX 80
#define RCX 88
#define RDX 96
#define RSI 104
#define RDI 112
#define ORIG_RAX 120       /* = ERROR */
/* end of arguments */
/* cpu exception frame or undefined in case of fast syscall. */
#define RIP 128
#define CS 136
#define EFLAGS 144
#define RSP 152
#define SS 160
#define ARGOFFSET R11
#endif /* __ASSEMBLY__ */

/* top of stack page */
#define FRAME_SIZE 168

#endif /* !__i386__ */

/* Arbitrarily choose the same ptrace numbers as used by the Sparc code. */
#define PTRACE_GETREGS            12
#define PTRACE_SETREGS            13
#define PTRACE_GETFPREGS          14
#define PTRACE_SETFPREGS          15
#define PTRACE_GETFPXREGS         18
#define PTRACE_SETFPXREGS         19

#define PTRACE_OLDSETOPTIONS      21

/* only useful for access 32bit programs / kernels */
#define PTRACE_GET_THREAD_AREA    25
#define PTRACE_SET_THREAD_AREA    26

#ifdef __x86_64__
# define PTRACE_ARCH_PRCTL	  30
#endif

#define PTRACE_SYSEMU		  31
#define PTRACE_SYSEMU_SINGLESTEP  32

#define PTRACE_SINGLEBLOCK	33	/* resume execution until next branch */

#ifdef CONFIG_X86_PTRACE_BTS

#ifndef __ASSEMBLY__
#include <linux/types.h>

struct ptrace_bts_config {
	/* requested or actual size of BTS buffer in bytes */
	__u32 size;
	/* bitmask of below flags */
	__u32 flags;
	/* buffer overflow signal */
	__u32 signal;
	/* actual size of bts_struct in bytes */
	__u32 bts_size;
};
#endif /* __ASSEMBLY__ */

#define PTRACE_BTS_O_TRACE	0x1 /* branch trace */
#define PTRACE_BTS_O_SCHED	0x2 /* scheduling events w/ jiffies */
#define PTRACE_BTS_O_SIGNAL     0x4 /* send SIG<signal> on buffer overflow
				       instead of wrapping around */
#define PTRACE_BTS_O_ALLOC	0x8 /* (re)allocate buffer */

#define PTRACE_BTS_CONFIG	40
#define PTRACE_BTS_STATUS	41
#define PTRACE_BTS_SIZE		42
#define PTRACE_BTS_GET		43
#define PTRACE_BTS_CLEAR	44
#define PTRACE_BTS_DRAIN	45
#endif /* CONFIG_X86_PTRACE_BTS */

#endif /* _ASM_X86_PTRACE_ABI_H */
