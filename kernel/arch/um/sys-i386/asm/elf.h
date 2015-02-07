
#ifndef __UM_ELF_I386_H
#define __UM_ELF_I386_H

#include <asm/user.h>
#include "skas.h"

#define R_386_NONE	0
#define R_386_32	1
#define R_386_PC32	2
#define R_386_GOT32	3
#define R_386_PLT32	4
#define R_386_COPY	5
#define R_386_GLOB_DAT	6
#define R_386_JMP_SLOT	7
#define R_386_RELATIVE	8
#define R_386_GOTOFF	9
#define R_386_GOTPC	10
#define R_386_NUM	11

typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof (struct user_regs_struct) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct user_i387_struct elf_fpregset_t;

#define elf_check_arch(x) \
	(((x)->e_machine == EM_386) || ((x)->e_machine == EM_486))

#define ELF_CLASS	ELFCLASS32
#define ELF_DATA        ELFDATA2LSB
#define ELF_ARCH        EM_386

#define ELF_PLAT_INIT(regs, load_addr) do { \
	PT_REGS_EBX(regs) = 0; \
	PT_REGS_ECX(regs) = 0; \
	PT_REGS_EDX(regs) = 0; \
	PT_REGS_ESI(regs) = 0; \
	PT_REGS_EDI(regs) = 0; \
	PT_REGS_EBP(regs) = 0; \
	PT_REGS_EAX(regs) = 0; \
} while (0)

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE 4096

#define ELF_ET_DYN_BASE (2 * TASK_SIZE / 3)

/* Shamelessly stolen from include/asm-i386/elf.h */

#define ELF_CORE_COPY_REGS(pr_reg, regs) do {	\
	pr_reg[0] = PT_REGS_EBX(regs);		\
	pr_reg[1] = PT_REGS_ECX(regs);		\
	pr_reg[2] = PT_REGS_EDX(regs);		\
	pr_reg[3] = PT_REGS_ESI(regs);		\
	pr_reg[4] = PT_REGS_EDI(regs);		\
	pr_reg[5] = PT_REGS_EBP(regs);		\
	pr_reg[6] = PT_REGS_EAX(regs);		\
	pr_reg[7] = PT_REGS_DS(regs);		\
	pr_reg[8] = PT_REGS_ES(regs);		\
	/* fake once used fs and gs selectors? */	\
	pr_reg[9] = PT_REGS_DS(regs);		\
	pr_reg[10] = PT_REGS_DS(regs);		\
	pr_reg[11] = PT_REGS_SYSCALL_NR(regs);	\
	pr_reg[12] = PT_REGS_IP(regs);		\
	pr_reg[13] = PT_REGS_CS(regs);		\
	pr_reg[14] = PT_REGS_EFLAGS(regs);	\
	pr_reg[15] = PT_REGS_SP(regs);		\
	pr_reg[16] = PT_REGS_SS(regs);		\
} while (0);

extern int elf_core_copy_fpregs(struct task_struct *t, elf_fpregset_t *fpu);

#define ELF_CORE_COPY_FPREGS(t, fpu) elf_core_copy_fpregs(t, fpu)

extern long elf_aux_hwcap;
#define ELF_HWCAP (elf_aux_hwcap)

extern char * elf_aux_platform;
#define ELF_PLATFORM (elf_aux_platform)

#define SET_PERSONALITY(ex) do { } while (0)

extern unsigned long vsyscall_ehdr;
extern unsigned long vsyscall_end;
extern unsigned long __kernel_vsyscall;

#define VSYSCALL_BASE vsyscall_ehdr
#define VSYSCALL_END vsyscall_end

#define FIXADDR_USER_START      VSYSCALL_BASE
#define FIXADDR_USER_END        VSYSCALL_END

#define AT_SYSINFO		32
#define AT_SYSINFO_EHDR		33

#define ARCH_DLINFO						\
do {								\
	if ( vsyscall_ehdr ) {					\
		NEW_AUX_ENT(AT_SYSINFO,	__kernel_vsyscall);	\
		NEW_AUX_ENT(AT_SYSINFO_EHDR, vsyscall_ehdr);	\
	}							\
} while (0)

#define ELF_CORE_EXTRA_PHDRS						      \
	(vsyscall_ehdr ? (((struct elfhdr *)vsyscall_ehdr)->e_phnum) : 0 )

#define ELF_CORE_WRITE_EXTRA_PHDRS					      \
if ( vsyscall_ehdr ) {							      \
	const struct elfhdr *const ehdrp = (struct elfhdr *)vsyscall_ehdr;    \
	const struct elf_phdr *const phdrp =				      \
		(const struct elf_phdr *) (vsyscall_ehdr + ehdrp->e_phoff);   \
	int i;								      \
	Elf32_Off ofs = 0;						      \
	for (i = 0; i < ehdrp->e_phnum; ++i) {				      \
		struct elf_phdr phdr = phdrp[i];			      \
		if (phdr.p_type == PT_LOAD) {				      \
			ofs = phdr.p_offset = offset;			      \
			offset += phdr.p_filesz;			      \
		}							      \
		else							      \
			phdr.p_offset += ofs;				      \
		phdr.p_paddr = 0; /* match other core phdrs */		      \
		DUMP_WRITE(&phdr, sizeof(phdr));			      \
	}								      \
}
#define ELF_CORE_WRITE_EXTRA_DATA					      \
if ( vsyscall_ehdr ) {							      \
	const struct elfhdr *const ehdrp = (struct elfhdr *)vsyscall_ehdr;    \
	const struct elf_phdr *const phdrp =				      \
		(const struct elf_phdr *) (vsyscall_ehdr + ehdrp->e_phoff);   \
	int i;								      \
	for (i = 0; i < ehdrp->e_phnum; ++i) {				      \
		if (phdrp[i].p_type == PT_LOAD)				      \
			DUMP_WRITE((void *) phdrp[i].p_vaddr,		      \
				   phdrp[i].p_filesz);			      \
	}								      \
}

#endif