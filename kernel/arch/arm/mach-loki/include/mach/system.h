

#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

#include <mach/hardware.h>
#include <mach/loki.h>

static inline void arch_idle(void)
{
	cpu_do_idle();
}

static inline void arch_reset(char mode)
{
	/*
	 * Enable soft reset to assert RSTOUTn.
	 */
	writel(SOFT_RESET_OUT_EN, RSTOUTn_MASK);

	/*
	 * Assert soft reset.
	 */
	writel(SOFT_RESET, SYSTEM_SOFT_RESET);

	while (1)
		;
}


#endif
