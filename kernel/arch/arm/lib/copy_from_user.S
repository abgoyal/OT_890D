

#include <linux/linkage.h>
#include <asm/assembler.h>


	.macro ldr1w ptr reg abort
100:	ldrt \reg, [\ptr], #4
	.section __ex_table, "a"
	.long 100b, \abort
	.previous
	.endm

	.macro ldr4w ptr reg1 reg2 reg3 reg4 abort
	ldr1w \ptr, \reg1, \abort
	ldr1w \ptr, \reg2, \abort
	ldr1w \ptr, \reg3, \abort
	ldr1w \ptr, \reg4, \abort
	.endm

	.macro ldr8w ptr reg1 reg2 reg3 reg4 reg5 reg6 reg7 reg8 abort
	ldr4w \ptr, \reg1, \reg2, \reg3, \reg4, \abort
	ldr4w \ptr, \reg5, \reg6, \reg7, \reg8, \abort
	.endm

	.macro ldr1b ptr reg cond=al abort
100:	ldr\cond\()bt \reg, [\ptr], #1
	.section __ex_table, "a"
	.long 100b, \abort
	.previous
	.endm

	.macro str1w ptr reg abort
	str \reg, [\ptr], #4
	.endm

	.macro str8w ptr reg1 reg2 reg3 reg4 reg5 reg6 reg7 reg8 abort
	stmia \ptr!, {\reg1, \reg2, \reg3, \reg4, \reg5, \reg6, \reg7, \reg8}
	.endm

	.macro str1b ptr reg cond=al abort
	str\cond\()b \reg, [\ptr], #1
	.endm

	.macro enter reg1 reg2
	mov	r3, #0
	stmdb	sp!, {r0, r2, r3, \reg1, \reg2}
	.endm

	.macro exit reg1 reg2
	add	sp, sp, #8
	ldmfd	sp!, {r0, \reg1, \reg2}
	.endm

	.text

ENTRY(__copy_from_user)

#include "copy_template.S"

ENDPROC(__copy_from_user)

	.section .fixup,"ax"
	.align 0
	copy_abort_preamble
	ldmfd	sp!, {r1, r2}
	sub	r3, r0, r1
	rsb	r1, r3, r2
	str	r1, [sp]
	bl	__memzero
	ldr	r0, [sp], #4
	copy_abort_end
	.previous

