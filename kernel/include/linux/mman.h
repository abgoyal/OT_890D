
#ifndef _LINUX_MMAN_H
#define _LINUX_MMAN_H

#include <asm/mman.h>

#define MREMAP_MAYMOVE	1
#define MREMAP_FIXED	2

#define OVERCOMMIT_GUESS		0
#define OVERCOMMIT_ALWAYS		1
#define OVERCOMMIT_NEVER		2

#ifdef __KERNEL__
#include <linux/mm.h>

#include <asm/atomic.h>

extern int sysctl_overcommit_memory;
extern int sysctl_overcommit_ratio;
extern atomic_long_t vm_committed_space;

#ifdef CONFIG_SMP
extern void vm_acct_memory(long pages);
#else
static inline void vm_acct_memory(long pages)
{
	atomic_long_add(pages, &vm_committed_space);
}
#endif

static inline void vm_unacct_memory(long pages)
{
	vm_acct_memory(-pages);
}


#ifndef arch_calc_vm_prot_bits
#define arch_calc_vm_prot_bits(prot) 0
#endif

#ifndef arch_vm_get_page_prot
#define arch_vm_get_page_prot(vm_flags) __pgprot(0)
#endif

#ifndef arch_validate_prot
static inline int arch_validate_prot(unsigned long prot)
{
	return (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC | PROT_SEM)) == 0;
}
#define arch_validate_prot arch_validate_prot
#endif

#define _calc_vm_trans(x, bit1, bit2) \
  ((bit1) <= (bit2) ? ((x) & (bit1)) * ((bit2) / (bit1)) \
   : ((x) & (bit1)) / ((bit1) / (bit2)))

static inline unsigned long
calc_vm_prot_bits(unsigned long prot)
{
	return _calc_vm_trans(prot, PROT_READ,  VM_READ ) |
	       _calc_vm_trans(prot, PROT_WRITE, VM_WRITE) |
	       _calc_vm_trans(prot, PROT_EXEC,  VM_EXEC) |
	       arch_calc_vm_prot_bits(prot);
}

static inline unsigned long
calc_vm_flag_bits(unsigned long flags)
{
	return _calc_vm_trans(flags, MAP_GROWSDOWN,  VM_GROWSDOWN ) |
	       _calc_vm_trans(flags, MAP_DENYWRITE,  VM_DENYWRITE ) |
	       _calc_vm_trans(flags, MAP_EXECUTABLE, VM_EXECUTABLE) |
	       _calc_vm_trans(flags, MAP_LOCKED,     VM_LOCKED    );
}
#endif /* __KERNEL__ */
#endif /* _LINUX_MMAN_H */
