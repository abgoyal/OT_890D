

#ifndef __ASM_ARCH_OMAP_OSK_H
#define __ASM_ARCH_OMAP_OSK_H

/* At OMAP5912 OSK the Ethernet is directly connected to CS1 */
#define OMAP_OSK_ETHR_START		0x04800300

#define OSK_TPS_GPIO_BASE		(OMAP_MAX_GPIO_LINES + 16 /* MPUIO */)
#	define OSK_TPS_GPIO_USB_PWR_EN	(OSK_TPS_GPIO_BASE + 0)
#	define OSK_TPS_GPIO_LED_D3	(OSK_TPS_GPIO_BASE + 1)
#	define OSK_TPS_GPIO_LAN_RESET	(OSK_TPS_GPIO_BASE + 2)
#	define OSK_TPS_GPIO_DSP_PWR_EN	(OSK_TPS_GPIO_BASE + 3)
#	define OSK_TPS_GPIO_LED_D9	(OSK_TPS_GPIO_BASE + 4)
#	define OSK_TPS_GPIO_LED_D2	(OSK_TPS_GPIO_BASE + 5)

#endif /*  __ASM_ARCH_OMAP_OSK_H */

