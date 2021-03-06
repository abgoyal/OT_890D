

#ifndef __ASM_ARCH_MAP_H
#define __ASM_ARCH_MAP_H

#include <plat/map-base.h>
#include <plat/map.h>

#define S3C2410_ADDR(x)		S3C_ADDR(x)

/* USB host controller */
#define S3C2410_PA_USBHOST (0x49000000)

/* DMA controller */
#define S3C2410_PA_DMA	   (0x4B000000)
#define S3C24XX_SZ_DMA	   SZ_1M

/* Clock and Power management */
#define S3C2410_PA_CLKPWR  (0x4C000000)

/* LCD controller */
#define S3C2410_PA_LCD	   (0x4D000000)
#define S3C24XX_SZ_LCD	   SZ_1M

/* NAND flash controller */
#define S3C2410_PA_NAND	   (0x4E000000)

/* IIC hardware controller */
#define S3C2410_PA_IIC	   (0x54000000)

/* IIS controller */
#define S3C2410_PA_IIS	   (0x55000000)

/* RTC */
#define S3C2410_PA_RTC	   (0x57000000)
#define S3C24XX_SZ_RTC	   SZ_1M

/* ADC */
#define S3C2410_PA_ADC	   (0x58000000)

/* SPI */
#define S3C2410_PA_SPI	   (0x59000000)

/* SDI */
#define S3C2410_PA_SDI	   (0x5A000000)

/* CAMIF */
#define S3C2440_PA_CAMIF   (0x4F000000)
#define S3C2440_SZ_CAMIF   SZ_1M

/* AC97 */

#define S3C2440_PA_AC97	   (0x5B000000)
#define S3C2440_SZ_AC97	   SZ_1M

/* S3C2443 High-speed SD/MMC */
#define S3C2443_PA_HSMMC   (0x4A800000)
#define S3C2443_SZ_HSMMC   (256)

/* physical addresses of all the chip-select areas */

#define S3C2410_CS0 (0x00000000)
#define S3C2410_CS1 (0x08000000)
#define S3C2410_CS2 (0x10000000)
#define S3C2410_CS3 (0x18000000)
#define S3C2410_CS4 (0x20000000)
#define S3C2410_CS5 (0x28000000)
#define S3C2410_CS6 (0x30000000)
#define S3C2410_CS7 (0x38000000)

#define S3C2410_SDRAM_PA    (S3C2410_CS6)

/* Use a single interface for common resources between S3C24XX cpus */

#define S3C24XX_PA_IRQ      S3C2410_PA_IRQ
#define S3C24XX_PA_MEMCTRL  S3C2410_PA_MEMCTRL
#define S3C24XX_PA_USBHOST  S3C2410_PA_USBHOST
#define S3C24XX_PA_DMA      S3C2410_PA_DMA
#define S3C24XX_PA_CLKPWR   S3C2410_PA_CLKPWR
#define S3C24XX_PA_LCD      S3C2410_PA_LCD
#define S3C24XX_PA_UART     S3C2410_PA_UART
#define S3C24XX_PA_TIMER    S3C2410_PA_TIMER
#define S3C24XX_PA_USBDEV   S3C2410_PA_USBDEV
#define S3C24XX_PA_WATCHDOG S3C2410_PA_WATCHDOG
#define S3C24XX_PA_IIS      S3C2410_PA_IIS
#define S3C24XX_PA_GPIO     S3C2410_PA_GPIO
#define S3C24XX_PA_RTC      S3C2410_PA_RTC
#define S3C24XX_PA_ADC      S3C2410_PA_ADC
#define S3C24XX_PA_SPI      S3C2410_PA_SPI
#define S3C24XX_PA_SDI      S3C2410_PA_SDI
#define S3C24XX_PA_NAND	    S3C2410_PA_NAND

#define S3C_PA_IIC          S3C2410_PA_IIC
#define S3C_PA_UART	    S3C24XX_PA_UART
#define S3C_PA_HSMMC0	    S3C2443_PA_HSMMC

#endif /* __ASM_ARCH_MAP_H */
