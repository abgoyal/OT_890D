

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/page.h>
#include <mach/msm_iomap.h>
#include <asm/mach/map.h>

#include <mach/board.h>

#define MSM_DEVICE(name) { \
		.virtual = (unsigned long) MSM_##name##_BASE, \
		.pfn = __phys_to_pfn(MSM_##name##_PHYS), \
		.length = MSM_##name##_SIZE, \
		.type = MT_DEVICE_NONSHARED, \
	 }

static struct map_desc msm_io_desc[] __initdata = {
	MSM_DEVICE(VIC),
	MSM_DEVICE(CSR),
	MSM_DEVICE(GPT),
	MSM_DEVICE(DMOV),
	MSM_DEVICE(GPIO1),
	MSM_DEVICE(GPIO2),
	MSM_DEVICE(CLK_CTL),
	{
		.virtual =  (unsigned long) MSM_SHARED_RAM_BASE,
		.pfn =      __phys_to_pfn(MSM_SHARED_RAM_PHYS),
		.length =   MSM_SHARED_RAM_SIZE,
		.type =     MT_DEVICE,
	},
};

void __init msm_map_common_io(void)
{
	/* Make sure the peripheral register window is closed, since
	 * we will use PTE flags (TEX[1]=1,B=0,C=1) to determine which
	 * pages are peripheral interface or not.
	 */
	asm("mcr p15, 0, %0, c15, c2, 4" : : "r" (0));

	iotable_init(msm_io_desc, ARRAY_SIZE(msm_io_desc));
}

void __iomem *
__msm_ioremap(unsigned long phys_addr, size_t size, unsigned int mtype)
{
	if (mtype == MT_DEVICE) {
		/* The peripherals in the 88000000 - D0000000 range
		 * are only accessable by type MT_DEVICE_NONSHARED.
		 * Adjust mtype as necessary to make this "just work."
		 */
		if ((phys_addr >= 0x88000000) && (phys_addr < 0xD0000000))
			mtype = MT_DEVICE_NONSHARED;
	}

	return __arm_ioremap(phys_addr, size, mtype);
}
