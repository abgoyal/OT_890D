

#ifndef _ASM_UNIT_CLOCK_H
#define _ASM_UNIT_CLOCK_H

#ifndef __ASSEMBLY__

#ifdef CONFIG_MN10300_RTC

extern unsigned long mn10300_ioclk;	/* IOCLK (crystal speed) in HZ */
extern unsigned long mn10300_iobclk;
extern unsigned long mn10300_tsc_per_HZ;

#define MN10300_IOCLK		((unsigned long)mn10300_ioclk)
/* If this processors has a another clock, uncomment the below. */
/* #define MN10300_IOBCLK	((unsigned long)mn10300_iobclk) */

#else /* !CONFIG_MN10300_RTC */

#define MN10300_IOCLK		33333333UL
/* #define MN10300_IOBCLK	66666666UL */

#endif /* !CONFIG_MN10300_RTC */

#define MN10300_JCCLK		MN10300_IOCLK
#define MN10300_TSCCLK		MN10300_IOCLK

#ifdef CONFIG_MN10300_RTC
#define MN10300_TSC_PER_HZ	((unsigned long)mn10300_tsc_per_HZ)
#else /* !CONFIG_MN10300_RTC */
#define MN10300_TSC_PER_HZ	(MN10300_TSCCLK/HZ)
#endif /* !CONFIG_MN10300_RTC */

#endif /* !__ASSEMBLY__ */

#endif /* _ASM_UNIT_CLOCK_H */
