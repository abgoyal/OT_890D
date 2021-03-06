

#ifndef ARCH_ARM_MACH_OMAP2_POWERDOMAINS24XX
#define ARCH_ARM_MACH_OMAP2_POWERDOMAINS24XX


#include <mach/powerdomain.h>

#include "prcm-common.h"
#include "prm.h"
#include "prm-regbits-24xx.h"
#include "cm.h"
#include "cm-regbits-24xx.h"

/* 24XX powerdomains and dependencies */

#ifdef CONFIG_ARCH_OMAP24XX


/* Wakeup dependency source arrays */

static struct pwrdm_dep dsp_mdm_24xx_wkdeps[] = {
	{
		.pwrdm_name = "core_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.pwrdm_name = "mpu_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.pwrdm_name = "wkup_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{ NULL },
};

static struct pwrdm_dep mpu_24xx_wkdeps[] = {
	{
		.pwrdm_name = "core_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.pwrdm_name = "dsp_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.pwrdm_name = "wkup_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.pwrdm_name = "mdm_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP2430)
	},
	{ NULL },
};

static struct pwrdm_dep core_24xx_wkdeps[] = {
	{
		.pwrdm_name = "dsp_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.pwrdm_name = "gfx_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.pwrdm_name = "mpu_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.pwrdm_name = "wkup_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.pwrdm_name = "mdm_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP2430)
	},
	{ NULL },
};


/* Powerdomains */

static struct powerdomain dsp_pwrdm = {
	.name		  = "dsp_pwrdm",
	.prcm_offs	  = OMAP24XX_DSP_MOD,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX),
	.dep_bit	  = OMAP24XX_PM_WKDEP_MPU_EN_DSP_SHIFT,
	.wkdep_srcs	  = dsp_mdm_24xx_wkdeps,
	.pwrsts		  = PWRSTS_OFF_RET_ON,
	.pwrsts_logic_ret = PWRDM_POWER_RET,
	.banks		  = 1,
	.pwrsts_mem_ret	  = {
		[0] = PWRDM_POWER_RET,
	},
	.pwrsts_mem_on	  = {
		[0] = PWRDM_POWER_ON,
	},
};

static struct powerdomain mpu_24xx_pwrdm = {
	.name		  = "mpu_pwrdm",
	.prcm_offs	  = MPU_MOD,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX),
	.dep_bit	  = OMAP24XX_EN_MPU_SHIFT,
	.wkdep_srcs	  = mpu_24xx_wkdeps,
	.pwrsts		  = PWRSTS_OFF_RET_ON,
	.pwrsts_logic_ret = PWRSTS_OFF_RET,
	.banks		  = 1,
	.pwrsts_mem_ret	  = {
		[0] = PWRDM_POWER_RET,
	},
	.pwrsts_mem_on	  = {
		[0] = PWRDM_POWER_ON,
	},
};

static struct powerdomain core_24xx_pwrdm = {
	.name		  = "core_pwrdm",
	.prcm_offs	  = CORE_MOD,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX),
	.wkdep_srcs	  = core_24xx_wkdeps,
	.pwrsts		  = PWRSTS_OFF_RET_ON,
	.dep_bit	  = OMAP24XX_EN_CORE_SHIFT,
	.banks		  = 3,
	.pwrsts_mem_ret	  = {
		[0] = PWRSTS_OFF_RET,	 /* MEM1RETSTATE */
		[1] = PWRSTS_OFF_RET,	 /* MEM2RETSTATE */
		[2] = PWRSTS_OFF_RET,	 /* MEM3RETSTATE */
	},
	.pwrsts_mem_on	  = {
		[0] = PWRSTS_OFF_RET_ON, /* MEM1ONSTATE */
		[1] = PWRSTS_OFF_RET_ON, /* MEM2ONSTATE */
		[2] = PWRSTS_OFF_RET_ON, /* MEM3ONSTATE */
	},
};

#endif	   /* CONFIG_ARCH_OMAP24XX */




#ifdef CONFIG_ARCH_OMAP2430

/* XXX 2430 KILLDOMAINWKUP bit?  No current users apparently */

/* Another case of bit name collisions between several registers: EN_MDM */
static struct powerdomain mdm_pwrdm = {
	.name		  = "mdm_pwrdm",
	.prcm_offs	  = OMAP2430_MDM_MOD,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
	.dep_bit	  = OMAP2430_PM_WKDEP_MPU_EN_MDM_SHIFT,
	.wkdep_srcs	  = dsp_mdm_24xx_wkdeps,
	.pwrsts		  = PWRSTS_OFF_RET_ON,
	.pwrsts_logic_ret = PWRDM_POWER_RET,
	.banks		  = 1,
	.pwrsts_mem_ret	  = {
		[0] = PWRDM_POWER_RET, /* MEMRETSTATE */
	},
	.pwrsts_mem_on	  = {
		[0] = PWRDM_POWER_ON,  /* MEMONSTATE */
	},
};

#endif     /* CONFIG_ARCH_OMAP2430 */


#endif
