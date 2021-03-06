
#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/hwcap.h>
#include <asm/pgtable-hwdef.h>
#include <asm/pgtable.h>
#include <asm/ptrace.h>

#include "proc-macros.S"

#define MAX_AREA_SIZE	32768

#define CACHE_DLINESIZE	32

#define CACHE_DSEGMENTS	16

#define CACHE_DENTRIES	64

#define CACHE_DLIMIT	32768

	.text
ENTRY(cpu_arm1020_proc_init)
	mov	pc, lr

ENTRY(cpu_arm1020_proc_fin)
	stmfd	sp!, {lr}
	mov	ip, #PSR_F_BIT | PSR_I_BIT | SVC_MODE
	msr	cpsr_c, ip
	bl	arm1020_flush_kern_cache_all
	mrc	p15, 0, r0, c1, c0, 0		@ ctrl register
	bic	r0, r0, #0x1000 		@ ...i............
	bic	r0, r0, #0x000e 		@ ............wca.
	mcr	p15, 0, r0, c1, c0, 0		@ disable caches
	ldmfd	sp!, {pc}

	.align	5
ENTRY(cpu_arm1020_reset)
	mov	ip, #0
	mcr	p15, 0, ip, c7, c7, 0		@ invalidate I,D caches
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
#ifdef CONFIG_MMU
	mcr	p15, 0, ip, c8, c7, 0		@ invalidate I & D TLBs
#endif
	mrc	p15, 0, ip, c1, c0, 0		@ ctrl register
	bic	ip, ip, #0x000f 		@ ............wcam
	bic	ip, ip, #0x1100 		@ ...i...s........
	mcr	p15, 0, ip, c1, c0, 0		@ ctrl register
	mov	pc, r0

	.align	5
ENTRY(cpu_arm1020_do_idle)
	mcr	p15, 0, r0, c7, c0, 4		@ Wait for interrupt
	mov	pc, lr

/* ================================= CACHE ================================ */

	.align	5
ENTRY(arm1020_flush_user_cache_all)
	/* FALLTHROUGH */
ENTRY(arm1020_flush_kern_cache_all)
	mov	r2, #VM_EXEC
	mov	ip, #0
__flush_whole_cache:
#ifndef CONFIG_CPU_DCACHE_DISABLE
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mov	r1, #(CACHE_DSEGMENTS - 1) << 5	@ 16 segments
1:	orr	r3, r1, #(CACHE_DENTRIES - 1) << 26 @ 64 entries
2:	mcr	p15, 0, r3, c7, c14, 2		@ clean+invalidate D index
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	subs	r3, r3, #1 << 26
	bcs	2b				@ entries 63 to 0
	subs	r1, r1, #1 << 5
	bcs	1b				@ segments 15 to 0
#endif
	tst	r2, #VM_EXEC
#ifndef CONFIG_CPU_ICACHE_DISABLE
	mcrne	p15, 0, ip, c7, c5, 0		@ invalidate I cache
#endif
	mcrne	p15, 0, ip, c7, c10, 4		@ drain WB
	mov	pc, lr

ENTRY(arm1020_flush_user_cache_range)
	mov	ip, #0
	sub	r3, r1, r0			@ calculate total size
	cmp	r3, #CACHE_DLIMIT
	bhs	__flush_whole_cache

#ifndef CONFIG_CPU_DCACHE_DISABLE
	mcr	p15, 0, ip, c7, c10, 4
1:	mcr	p15, 0, r0, c7, c14, 1		@ clean+invalidate D entry
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	add	r0, r0, #CACHE_DLINESIZE
	cmp	r0, r1
	blo	1b
#endif
	tst	r2, #VM_EXEC
#ifndef CONFIG_CPU_ICACHE_DISABLE
	mcrne	p15, 0, ip, c7, c5, 0		@ invalidate I cache
#endif
	mcrne	p15, 0, ip, c7, c10, 4		@ drain WB
	mov	pc, lr

ENTRY(arm1020_coherent_kern_range)
	/* FALLTRHOUGH */

ENTRY(arm1020_coherent_user_range)
	mov	ip, #0
	bic	r0, r0, #CACHE_DLINESIZE - 1
	mcr	p15, 0, ip, c7, c10, 4
1:
#ifndef CONFIG_CPU_DCACHE_DISABLE
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
#endif
#ifndef CONFIG_CPU_ICACHE_DISABLE
	mcr	p15, 0, r0, c7, c5, 1		@ invalidate I entry
#endif
	add	r0, r0, #CACHE_DLINESIZE
	cmp	r0, r1
	blo	1b
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mov	pc, lr

ENTRY(arm1020_flush_kern_dcache_page)
	mov	ip, #0
#ifndef CONFIG_CPU_DCACHE_DISABLE
	add	r1, r0, #PAGE_SZ
1:	mcr	p15, 0, r0, c7, c14, 1		@ clean+invalidate D entry
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	add	r0, r0, #CACHE_DLINESIZE
	cmp	r0, r1
	blo	1b
#endif
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mov	pc, lr

ENTRY(arm1020_dma_inv_range)
	mov	ip, #0
#ifndef CONFIG_CPU_DCACHE_DISABLE
	tst	r0, #CACHE_DLINESIZE - 1
	bic	r0, r0, #CACHE_DLINESIZE - 1
	mcrne	p15, 0, ip, c7, c10, 4
	mcrne	p15, 0, r0, c7, c10, 1		@ clean D entry
	mcrne	p15, 0, ip, c7, c10, 4		@ drain WB
	tst	r1, #CACHE_DLINESIZE - 1
	mcrne	p15, 0, ip, c7, c10, 4
	mcrne	p15, 0, r1, c7, c10, 1		@ clean D entry
	mcrne	p15, 0, ip, c7, c10, 4		@ drain WB
1:	mcr	p15, 0, r0, c7, c6, 1		@ invalidate D entry
	add	r0, r0, #CACHE_DLINESIZE
	cmp	r0, r1
	blo	1b
#endif
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mov	pc, lr

ENTRY(arm1020_dma_clean_range)
	mov	ip, #0
#ifndef CONFIG_CPU_DCACHE_DISABLE
	bic	r0, r0, #CACHE_DLINESIZE - 1
1:	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	add	r0, r0, #CACHE_DLINESIZE
	cmp	r0, r1
	blo	1b
#endif
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mov	pc, lr

ENTRY(arm1020_dma_flush_range)
	mov	ip, #0
#ifndef CONFIG_CPU_DCACHE_DISABLE
	bic	r0, r0, #CACHE_DLINESIZE - 1
	mcr	p15, 0, ip, c7, c10, 4
1:	mcr	p15, 0, r0, c7, c14, 1		@ clean+invalidate D entry
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	add	r0, r0, #CACHE_DLINESIZE
	cmp	r0, r1
	blo	1b
#endif
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mov	pc, lr

ENTRY(arm1020_cache_fns)
	.long	arm1020_flush_kern_cache_all
	.long	arm1020_flush_user_cache_all
	.long	arm1020_flush_user_cache_range
	.long	arm1020_coherent_kern_range
	.long	arm1020_coherent_user_range
	.long	arm1020_flush_kern_dcache_page
	.long	arm1020_dma_inv_range
	.long	arm1020_dma_clean_range
	.long	arm1020_dma_flush_range

	.align	5
ENTRY(cpu_arm1020_dcache_clean_area)
#ifndef CONFIG_CPU_DCACHE_DISABLE
	mov	ip, #0
1:	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	add	r0, r0, #CACHE_DLINESIZE
	subs	r1, r1, #CACHE_DLINESIZE
	bhi	1b
#endif
	mov	pc, lr

/* =============================== PageTable ============================== */

	.align	5
ENTRY(cpu_arm1020_switch_mm)
#ifdef CONFIG_MMU
#ifndef CONFIG_CPU_DCACHE_DISABLE
	mcr	p15, 0, r3, c7, c10, 4
	mov	r1, #0xF			@ 16 segments
1:	mov	r3, #0x3F			@ 64 entries
2:	mov	ip, r3, LSL #26 		@ shift up entry
	orr	ip, ip, r1, LSL #5		@ shift in/up index
	mcr	p15, 0, ip, c7, c14, 2		@ Clean & Inval DCache entry
	mov	ip, #0
	mcr	p15, 0, ip, c7, c10, 4
	subs	r3, r3, #1
	cmp	r3, #0
	bge	2b				@ entries 3F to 0
	subs	r1, r1, #1
	cmp	r1, #0
	bge	1b				@ segments 15 to 0

#endif
	mov	r1, #0
#ifndef CONFIG_CPU_ICACHE_DISABLE
	mcr	p15, 0, r1, c7, c5, 0		@ invalidate I cache
#endif
	mcr	p15, 0, r1, c7, c10, 4		@ drain WB
	mcr	p15, 0, r0, c2, c0, 0		@ load page table pointer
	mcr	p15, 0, r1, c8, c7, 0		@ invalidate I & D TLBs
#endif /* CONFIG_MMU */
	mov	pc, lr
        
	.align	5
ENTRY(cpu_arm1020_set_pte_ext)
#ifdef CONFIG_MMU
	armv3_set_pte_ext
	mov	r0, r0
#ifndef CONFIG_CPU_DCACHE_DISABLE
	mcr	p15, 0, r0, c7, c10, 4
	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
#endif
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
#endif /* CONFIG_MMU */
	mov	pc, lr

	__INIT

	.type	__arm1020_setup, #function
__arm1020_setup:
	mov	r0, #0
	mcr	p15, 0, r0, c7, c7		@ invalidate I,D caches on v4
	mcr	p15, 0, r0, c7, c10, 4		@ drain write buffer on v4
#ifdef CONFIG_MMU
	mcr	p15, 0, r0, c8, c7		@ invalidate I,D TLBs on v4
#endif

	adr	r5, arm1020_crval
	ldmia	r5, {r5, r6}
	mrc	p15, 0, r0, c1, c0		@ get control register v4
	bic	r0, r0, r5
	orr	r0, r0, r6
#ifdef CONFIG_CPU_CACHE_ROUND_ROBIN
	orr	r0, r0, #0x4000 		@ .R.. .... .... ....
#endif
	mov	pc, lr
	.size	__arm1020_setup, . - __arm1020_setup

	/*
	 *  R
	 * .RVI ZFRS BLDP WCAM
	 * .011 1001 ..11 0101
	 */
	.type	arm1020_crval, #object
arm1020_crval:
	crval	clear=0x0000593f, mmuset=0x00003935, ucset=0x00001930

	__INITDATA

	.type	arm1020_processor_functions, #object
arm1020_processor_functions:
	.word	v4t_early_abort
	.word	pabort_noifar
	.word	cpu_arm1020_proc_init
	.word	cpu_arm1020_proc_fin
	.word	cpu_arm1020_reset
	.word	cpu_arm1020_do_idle
	.word	cpu_arm1020_dcache_clean_area
	.word	cpu_arm1020_switch_mm
	.word	cpu_arm1020_set_pte_ext
	.size	arm1020_processor_functions, . - arm1020_processor_functions

	.section ".rodata"

	.type	cpu_arch_name, #object
cpu_arch_name:
	.asciz	"armv5t"
	.size	cpu_arch_name, . - cpu_arch_name

	.type	cpu_elf_name, #object
cpu_elf_name:
	.asciz	"v5"
	.size	cpu_elf_name, . - cpu_elf_name

	.type	cpu_arm1020_name, #object
cpu_arm1020_name:
	.ascii	"ARM1020"
#ifndef CONFIG_CPU_ICACHE_DISABLE
	.ascii	"i"
#endif
#ifndef CONFIG_CPU_DCACHE_DISABLE
	.ascii	"d"
#ifdef CONFIG_CPU_DCACHE_WRITETHROUGH
	.ascii	"(wt)"
#else
	.ascii	"(wb)"
#endif
#endif
#ifndef CONFIG_CPU_BPREDICT_DISABLE
	.ascii	"B"
#endif
#ifdef CONFIG_CPU_CACHE_ROUND_ROBIN
	.ascii	"RR"
#endif
	.ascii	"\0"
	.size	cpu_arm1020_name, . - cpu_arm1020_name

	.align

	.section ".proc.info.init", #alloc, #execinstr

	.type	__arm1020_proc_info,#object
__arm1020_proc_info:
	.long	0x4104a200			@ ARM 1020T (Architecture v5T)
	.long	0xff0ffff0
	.long   PMD_TYPE_SECT | \
		PMD_SECT_AP_WRITE | \
		PMD_SECT_AP_READ
	.long   PMD_TYPE_SECT | \
		PMD_SECT_AP_WRITE | \
		PMD_SECT_AP_READ
	b	__arm1020_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP | HWCAP_HALF | HWCAP_THUMB
	.long	cpu_arm1020_name
	.long	arm1020_processor_functions
	.long	v4wbi_tlb_fns
	.long	v4wb_user_fns
	.long	arm1020_cache_fns
	.size	__arm1020_proc_info, . - __arm1020_proc_info
