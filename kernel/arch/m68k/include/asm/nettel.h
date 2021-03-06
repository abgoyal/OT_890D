
/****************************************************************************/


/****************************************************************************/
#ifndef	nettel_h
#define	nettel_h
/****************************************************************************/


/****************************************************************************/
#ifdef CONFIG_NETtel
/****************************************************************************/

#ifdef CONFIG_COLDFIRE
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#endif

/*---------------------------------------------------------------------------*/
#if defined(CONFIG_M5307)
#define	MCFPP_DCD1	0x0001
#define	MCFPP_DCD0	0x0002
#define	MCFPP_DTR1	0x0004
#define	MCFPP_DTR0	0x0008

#define	NETtel_LEDADDR	0x30400000

#ifndef __ASSEMBLY__

extern volatile unsigned short ppdata;

static __inline__ unsigned int mcf_getppdata(void)
{
	volatile unsigned short *pp;
	pp = (volatile unsigned short *) (MCF_MBAR + MCFSIM_PADAT);
	return((unsigned int) *pp);
}

static __inline__ void mcf_setppdata(unsigned int mask, unsigned int bits)
{
	volatile unsigned short *pp;
	pp = (volatile unsigned short *) (MCF_MBAR + MCFSIM_PADAT);
	ppdata = (ppdata & ~mask) | bits;
	*pp = ppdata;
}
#endif

/*---------------------------------------------------------------------------*/
#elif defined(CONFIG_M5206e)
#define	NETtel_LEDADDR	0x50000000

/*---------------------------------------------------------------------------*/
#elif defined(CONFIG_M5272)
#define	MCFPP_DCD0	0x0080
#define	MCFPP_DCD1	0x0000		/* Port 1 no DCD support */
#define	MCFPP_DTR0	0x0040
#define	MCFPP_DTR1	0x0000		/* Port 1 no DTR support */

#ifndef __ASSEMBLY__
static __inline__ unsigned int mcf_getppdata(void)
{
	volatile unsigned short *pp;
	pp = (volatile unsigned short *) (MCF_MBAR + MCFSIM_PBDAT);
	return((unsigned int) *pp);
}

static __inline__ void mcf_setppdata(unsigned int mask, unsigned int bits)
{
	volatile unsigned short *pp;
	pp = (volatile unsigned short *) (MCF_MBAR + MCFSIM_PBDAT);
	*pp = (*pp & ~mask) | bits;
}
#endif

#endif
/*---------------------------------------------------------------------------*/

/****************************************************************************/
#endif /* CONFIG_NETtel */
/****************************************************************************/
#endif	/* nettel_h */
