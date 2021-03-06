


#ifndef __PDN_HW_H__
#define __PDN_HW_H__

#include    <mach/mt3351_typedefs.h>
#include    <mach/mt3351_reg_base.h>



// clock and PDN register

#define SW_MISC              (CONFIG_BASE+0x0010)
#define HW_MISC              (CONFIG_BASE+0x0020)
#define CGCON                (CONFIG_BASE+0x0204) /*CPU to DSP reset register */
#define CLKCON				 (CONFIG_BASE+0x0208)
#define PDNCONA              (CONFIG_BASE+0x0300)
#define PDNCONB              (CONFIG_BASE+0x0304)
#define PDNSETA              (CONFIG_BASE+0x0320)
#define PDNSETB              (CONFIG_BASE+0x0324)
#define PDNCLRA              (CONFIG_BASE+0x0340)
#define PDNCLRB              (CONFIG_BASE+0x0344)

// HW_MISC
#define CKEN_HOLD_EN        0x40000000
#define FAST_EN             0x80000
#define ARMDCM				0x40000
#define ARMCG				0x20000
#define ASYNC				0x10000
#define GMC_ACG				0x2

//CGCON
#define DCM_EN				0x2
#define DSP_OFF				0x4
#define MM_OFF				0x8
#define EMI2X_OFF			0x10
#define UPLL_48M_OFF		0x80


// CLKCON
#define DVFS_UPD			0x80000000
#define CLK_DLY_MASK		0x700000
#define EMI_CLKDIV_MASK		0xC0000		
#define ARM_CLKDIV_MASK			0x30000
#define MMCKSRC_MASK				0xC0
#define ARMCKSRC_MASK			0x30
#define DCM_FSEL_MASK			0x7

typedef enum
{
	EMI_DIV_1 = 0,
	EMI_DIV_2 = 1,
	EMI_DIV_4 = 3,
}EMI_CLK_DIVISOR;


typedef enum
{
	ARM_DIV_1 = 0,
	ARM_DIV_2,
	ARM_DIV_1_3,
	ARM_DIV_4
}ARM_CLK_DIVISOR;


typedef enum
{
    MCU_CLK_SOURCE = 0,
    MPLL_DIVIDE_2,
	MM_CLK_RESERVED
}MM_CLK_SRC;


typedef enum
{
    MCU_CLK_EXTERNAL = 0,
    MCU_CLK_UPLL,
    MCU_CLK_MPLL,
    MCU_CLK_APLL
}MCU_CLK_SRC;

typedef enum
{
    MCU_SYNC_MODE = 0,
    MCU_ASYNC_MODE
} MCU_MODE;

typedef enum
{
    AHB_DIVIDE_8_APB_DIVIDE_4 = 0,
    AHB_DIVIDE_4_APB_DIVIDE_2 = 1,
    AHB_DIVIDE_2_APB_DIVIDE_1 = 3,
    AHB_DIVIDE_1_APB_DIVIDE_1 = 7,    
} DCM_FSEL;


// PDNCONA
// PDNSETA
// PDNCLRA

// PDNCONB
// PDNSETB
// PDNCLRB

void Set_CLK_CONTROL(EMI_CLK_DIVISOR emi_div, ARM_CLK_DIVISOR arm_div);
void Choose_ARM_CLK_SRC(MCU_CLK_SRC clk);
void Set_CLK_DLY(UINT8 delay);
void ConfigGMCAsyncMode(void);
void Set_MCU_MM_Mode(MCU_MODE mode);
void Choose_MM_CLK_SRC(MM_CLK_SRC clk);

#endif /* __PDN_HW_H__ */
