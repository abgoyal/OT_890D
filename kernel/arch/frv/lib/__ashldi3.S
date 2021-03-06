

        .text
        .p2align	4

###############################################################################
#
# unsigned long long __ashldi3(unsigned long long value [GR8:GR9], unsigned by [GR10])
#
###############################################################################
        .globl		__ashldi3
        .type		__ashldi3,@function
__ashldi3:
	andicc.p	gr10,#63,gr10,icc0
	setlos		#32,gr5
	andicc.p	gr10,#32,gr0,icc1
	beqlr		icc0,#0
	ckeq		icc1,cc4			; cc4 is true if 0<N<32

	# deal with a shift in the range 1<=N<=31
	csll.p		gr8,gr10,gr8	,cc4,#1		; MSW <<= N
	csub		gr5,gr10,gr5	,cc4,#1		; M = 32 - N
	csrl.p		gr9,gr5,gr4	,cc4,#1
	csll		gr9,gr10,gr9	,cc4,#1		; LSW <<= N
	cor.p		gr4,gr8,gr8	,cc4,#1		; MSW |= LSW >> M

	# deal with a shift in the range 32<=N<=63
	csll		gr9,gr10,gr8	,cc4,#0		; MSW = LSW << (N & 31 [implicit AND])
	cor.p		gr0,gr0,gr9	,cc4,#0		; LSW = 0
	bralr
	.size		__ashldi3, .-__ashldi3
