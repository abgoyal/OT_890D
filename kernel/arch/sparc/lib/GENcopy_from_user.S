

#define EX_LD(x)		\
98:	x;			\
	.section __ex_table,"a";\
	.align 4;		\
	.word 98b, __retl_one;	\
	.text;			\
	.align 4;

#ifndef ASI_AIUS
#define ASI_AIUS	0x11
#endif

#define FUNC_NAME		GENcopy_from_user
#define LOAD(type,addr,dest)	type##a [addr] ASI_AIUS, dest
#define EX_RETVAL(x)		0

#ifdef __KERNEL__
#define PREAMBLE					\
	rd		%asi, %g1;			\
	cmp		%g1, ASI_AIUS;			\
	bne,pn		%icc, ___copy_in_user;		\
	 nop
#endif

#include "GENmemcpy.S"
