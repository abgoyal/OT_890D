
#ifndef _ASM_POWERPC_MMU_H_
#define _ASM_POWERPC_MMU_H_
#ifdef __KERNEL__

#include <asm/asm-compat.h>
#include <asm/feature-fixups.h>


#define MMU_FTR_HPTE_TABLE		ASM_CONST(0x00000001)
#define MMU_FTR_TYPE_8xx		ASM_CONST(0x00000002)
#define MMU_FTR_TYPE_40x		ASM_CONST(0x00000004)
#define MMU_FTR_TYPE_44x		ASM_CONST(0x00000008)
#define MMU_FTR_TYPE_FSL_E		ASM_CONST(0x00000010)


/* Enable use of high BAT registers */
#define MMU_FTR_USE_HIGH_BATS		ASM_CONST(0x00010000)

#define MMU_FTR_BIG_PHYS		ASM_CONST(0x00020000)

#define MMU_FTR_USE_TLBIVAX_BCAST	ASM_CONST(0x00040000)

#define MMU_FTR_USE_TLBILX_PID		ASM_CONST(0x00080000)

#define MMU_FTR_LOCK_BCAST_INVAL	ASM_CONST(0x00100000)

#ifndef __ASSEMBLY__
#include <asm/cputable.h>

static inline int mmu_has_feature(unsigned long feature)
{
	return (cur_cpu_spec->mmu_features & feature);
}

extern unsigned int __start___mmu_ftr_fixup, __stop___mmu_ftr_fixup;

#endif /* !__ASSEMBLY__ */


#ifdef CONFIG_PPC64
/* 64-bit classic hash table MMU */
#  include <asm/mmu-hash64.h>
#elif defined(CONFIG_PPC_STD_MMU)
/* 32-bit classic hash table MMU */
#  include <asm/mmu-hash32.h>
#elif defined(CONFIG_40x)
/* 40x-style software loaded TLB */
#  include <asm/mmu-40x.h>
#elif defined(CONFIG_44x)
/* 44x-style software loaded TLB */
#  include <asm/mmu-44x.h>
#elif defined(CONFIG_FSL_BOOKE)
/* Freescale Book-E software loaded TLB */
#  include <asm/mmu-fsl-booke.h>
#elif defined (CONFIG_PPC_8xx)
/* Motorola/Freescale 8xx software loaded TLB */
#  include <asm/mmu-8xx.h>
#endif

#endif /* __KERNEL__ */
#endif /* _ASM_POWERPC_MMU_H_ */
