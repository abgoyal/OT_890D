

#ifndef __ARCH_ARM_MACH_OMAP2_CLOCKDOMAINS_H
#define __ARCH_ARM_MACH_OMAP2_CLOCKDOMAINS_H

#include <mach/clockdomain.h>


/* This is an implicit clockdomain - it is never defined as such in TRM */
static struct clockdomain wkup_clkdm = {
	.name		= "wkup_clkdm",
	.pwrdm_name	= "wkup_pwrdm",
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP24XX | CHIP_IS_OMAP3430),
};


#if defined(CONFIG_ARCH_OMAP2420)

static struct clockdomain mpu_2420_clkdm = {
	.name		= "mpu_clkdm",
	.pwrdm_name	= "mpu_pwrdm",
	.flags		= CLKDM_CAN_HWSUP,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_MPU_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain iva1_2420_clkdm = {
	.name		= "iva1_clkdm",
	.pwrdm_name	= "dsp_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP2420_AUTOSTATE_IVA_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

#endif  /* CONFIG_ARCH_OMAP2420 */



#if defined(CONFIG_ARCH_OMAP2430)

static struct clockdomain mpu_2430_clkdm = {
	.name		= "mpu_clkdm",
	.pwrdm_name	= "mpu_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_MPU_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

static struct clockdomain mdm_clkdm = {
	.name		= "mdm_clkdm",
	.pwrdm_name	= "mdm_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP2430_AUTOSTATE_MDM_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

#endif    /* CONFIG_ARCH_OMAP2430 */



#if defined(CONFIG_ARCH_OMAP24XX)

static struct clockdomain dsp_clkdm = {
	.name		= "dsp_clkdm",
	.pwrdm_name	= "dsp_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_DSP_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP24XX),
};

static struct clockdomain gfx_24xx_clkdm = {
	.name		= "gfx_clkdm",
	.pwrdm_name	= "gfx_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_GFX_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP24XX),
};

static struct clockdomain core_l3_24xx_clkdm = {
	.name		= "core_l3_clkdm",
	.pwrdm_name	= "core_pwrdm",
	.flags		= CLKDM_CAN_HWSUP,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_L3_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP24XX),
};

static struct clockdomain core_l4_24xx_clkdm = {
	.name		= "core_l4_clkdm",
	.pwrdm_name	= "core_pwrdm",
	.flags		= CLKDM_CAN_HWSUP,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_L4_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP24XX),
};

static struct clockdomain dss_24xx_clkdm = {
	.name		= "dss_clkdm",
	.pwrdm_name	= "core_pwrdm",
	.flags		= CLKDM_CAN_HWSUP,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_DSS_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP24XX),
};

#endif   /* CONFIG_ARCH_OMAP24XX */



#if defined(CONFIG_ARCH_OMAP34XX)

static struct clockdomain mpu_34xx_clkdm = {
	.name		= "mpu_clkdm",
	.pwrdm_name	= "mpu_pwrdm",
	.flags		= CLKDM_CAN_HWSUP | CLKDM_CAN_FORCE_WAKEUP,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_MPU_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain neon_clkdm = {
	.name		= "neon_clkdm",
	.pwrdm_name	= "neon_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_NEON_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain iva2_clkdm = {
	.name		= "iva2_clkdm",
	.pwrdm_name	= "iva2_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_IVA2_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain gfx_3430es1_clkdm = {
	.name		= "gfx_clkdm",
	.pwrdm_name	= "gfx_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP3430ES1_CLKTRCTRL_GFX_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430ES1),
};

static struct clockdomain sgx_clkdm = {
	.name		= "sgx_clkdm",
	.pwrdm_name	= "sgx_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP3430ES2_CLKTRCTRL_SGX_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430ES2),
};

static struct clockdomain d2d_clkdm = {
	.name		= "d2d_clkdm",
	.pwrdm_name	= "core_pwrdm",
	.flags		= CLKDM_CAN_HWSUP,
	.clktrctrl_mask = OMAP3430ES1_CLKTRCTRL_D2D_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain core_l3_34xx_clkdm = {
	.name		= "core_l3_clkdm",
	.pwrdm_name	= "core_pwrdm",
	.flags		= CLKDM_CAN_HWSUP,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_L3_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain core_l4_34xx_clkdm = {
	.name		= "core_l4_clkdm",
	.pwrdm_name	= "core_pwrdm",
	.flags		= CLKDM_CAN_HWSUP,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_L4_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain dss_34xx_clkdm = {
	.name		= "dss_clkdm",
	.pwrdm_name	= "dss_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_DSS_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain cam_clkdm = {
	.name		= "cam_clkdm",
	.pwrdm_name	= "cam_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_CAM_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain usbhost_clkdm = {
	.name		= "usbhost_clkdm",
	.pwrdm_name	= "usbhost_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP3430ES2_CLKTRCTRL_USBHOST_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430ES2),
};

static struct clockdomain per_clkdm = {
	.name		= "per_clkdm",
	.pwrdm_name	= "per_pwrdm",
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_PER_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain emu_clkdm = {
	.name		= "emu_clkdm",
	.pwrdm_name	= "emu_pwrdm",
	.flags		= CLKDM_CAN_ENABLE_AUTO | CLKDM_CAN_SWSUP,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_EMU_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

#endif   /* CONFIG_ARCH_OMAP34XX */


static struct clkdm_pwrdm_autodep clkdm_pwrdm_autodeps[] = {
	{
		.pwrdm_name = "mpu_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.pwrdm_name = "iva2_pwrdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL }
};


static struct clockdomain *clockdomains_omap[] = {

	&wkup_clkdm,

#ifdef CONFIG_ARCH_OMAP2420
	&mpu_2420_clkdm,
	&iva1_2420_clkdm,
#endif

#ifdef CONFIG_ARCH_OMAP2430
	&mpu_2430_clkdm,
	&mdm_clkdm,
#endif

#ifdef CONFIG_ARCH_OMAP24XX
	&dsp_clkdm,
	&gfx_24xx_clkdm,
	&core_l3_24xx_clkdm,
	&core_l4_24xx_clkdm,
	&dss_24xx_clkdm,
#endif

#ifdef CONFIG_ARCH_OMAP34XX
	&mpu_34xx_clkdm,
	&neon_clkdm,
	&iva2_clkdm,
	&gfx_3430es1_clkdm,
	&sgx_clkdm,
	&d2d_clkdm,
	&core_l3_34xx_clkdm,
	&core_l4_34xx_clkdm,
	&dss_34xx_clkdm,
	&cam_clkdm,
	&usbhost_clkdm,
	&per_clkdm,
	&emu_clkdm,
#endif

	NULL,
};

#endif
