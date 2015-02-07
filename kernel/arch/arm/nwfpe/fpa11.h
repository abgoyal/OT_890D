

#ifndef __FPA11_H__
#define __FPA11_H__

#define GET_FPA11() ((FPA11 *)(&current_thread_info()->fpstate))

#define GET_USERREG() ((struct pt_regs *)(THREAD_START_SP + (unsigned long)current_thread_info()) - 1)

#include <linux/thread_info.h>

/* includes */
#include "fpsr.h"		/* FP control and status register definitions */
#include "milieu.h"

struct roundingData {
    int8 mode;
    int8 precision;
    signed char exception;
};

#include "softfloat.h"

#define		typeNone		0x00
#define		typeSingle		0x01
#define		typeDouble		0x02
#define		typeExtended		0x03

typedef union tagFPREG {
	float32 fSingle;
	float64 fDouble;
#ifdef CONFIG_FPE_NWFPE_XP
	floatx80 fExtended;
#else
	u32 padding[3];
#endif
} __attribute__ ((packed,aligned(4))) FPREG;

typedef struct tagFPA11 {
/*   0 */ FPREG fpreg[8];	/* 8 floating point registers */
/*  96 */ FPSR fpsr;		/* floating point status register */
/* 100 */ FPCR fpcr;		/* floating point control register */
/* 104 */ unsigned char fType[8];	/* type of floating point value held in
					   floating point registers.  One of
					   none, single, double or extended. */
/* 112 */ int initflag;		/* this is special.  The kernel guarantees
				   to set it to 0 when a thread is launched,
				   so we can use it to detect whether this
				   instance of the emulator needs to be
				   initialised. */
} __attribute__ ((packed,aligned(4))) FPA11;

extern int8 SetRoundingMode(const unsigned int);
extern int8 SetRoundingPrecision(const unsigned int);
extern void nwfpe_init_fpa(union fp_state *fp);

extern unsigned int EmulateAll(unsigned int opcode);

extern unsigned int EmulateCPDT(const unsigned int opcode);
extern unsigned int EmulateCPDO(const unsigned int opcode);
extern unsigned int EmulateCPRT(const unsigned int opcode);

/* fpa11_cpdt.c */
extern unsigned int PerformLDF(const unsigned int opcode);
extern unsigned int PerformSTF(const unsigned int opcode);
extern unsigned int PerformLFM(const unsigned int opcode);
extern unsigned int PerformSFM(const unsigned int opcode);

/* single_cpdo.c */

extern unsigned int SingleCPDO(struct roundingData *roundData,
			       const unsigned int opcode, FPREG * rFd);
/* double_cpdo.c */
extern unsigned int DoubleCPDO(struct roundingData *roundData,
			       const unsigned int opcode, FPREG * rFd);

#endif