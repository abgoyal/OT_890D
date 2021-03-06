

#ifndef MT6516_SD_H
#define MT6516_SD_H

#include <linux/bitops.h>
#include <linux/mmc/host.h>

#include <mach/mt6516_reg_base.h>
#include <mach/dma.h>

/* MSDC registers */
#define MSDC_CFG                        ((volatile u32*)(base+0x0))
#define MSDC_STA                        ((volatile u32*)(base+0x04))
#define MSDC_INT                        ((volatile u32*)(base+0x08))
#define MSDC_DAT                        ((volatile u32*)(base+0x0c))
#define MSDC_PS                         ((volatile u32*)(base+0x10))
#define MSDC_IOCON                      ((volatile u32*)(base+0x14))
/* SDC registers */
#define SDC_CFG                         ((volatile u32*)(base+0x20))
#define SDC_CMD                         ((volatile u32*)(base+0x24))
#define SDC_ARG                         ((volatile u32*)(base+0x28))
#define SDC_STA                         ((volatile u32*)(base+0x2c)) // 16bits
#define SDC_RESP0                       ((volatile u32*)(base+0x30))
#define SDC_RESP1                       ((volatile u32*)(base+0x34))
#define SDC_RESP2                       ((volatile u32*)(base+0x38))
#define SDC_RESP3                       ((volatile u32*)(base+0x3c))
#define SDC_CMDSTA                      ((volatile u32*)(base+0x40)) // 16bits
#define SDC_DATSTA                      ((volatile u32*)(base+0x44)) // 16bits
#define SDC_CSTA                        ((volatile u32*)(base+0x48))
#define SDC_IRQMASK0                    ((volatile u32*)(base+0x4c))
#define SDC_IRQMASK1                    ((volatile u32*)(base+0x50))
/* SDIO registers */
#define SDIO_CFG                        ((volatile u32*)(base+0x54))
#define SDIO_STA                        ((volatile u32*)(base+0x58))

/* Masks for MSDC_CFG */
#define MSDC_CFG_MSDC                   (0x1 << 0)      /* R/W */
#define MSDC_CFG_NOCRC                  (0x1 << 2)      /* R/W */
#define MSDC_CFG_RST                    (0x1 << 3)      /* W */
#define MSDC_CFG_CLKSRC                 (0x1 << 4)      /* R/W */
#define MSDC_CFG_STDBY                  (0x1 << 5)      /* R/W */
#define MSDC_CFG_RED                    (0x1 << 6)      /* R/W */
#define MSDC_CFG_SCLKON                 (0x1 << 7)      /* R/W */
#define MSDC_CFG_SCLKF                  (0xff << 8)     /* R/W */
#define MSDC_CFG_INTEN                  (0x1 << 16)     /* R/W */
#define MSDC_CFG_DMAEN                  (0x1 << 17)     /* R/W */
#define MSDC_CFG_PINEN                  (0x1 << 18)     /* R/W */
#define MSDC_CFG_DIRQE                  (0x1 << 19)     /* R/W */
#define MSDC_CFG_RCDEN                  (0x1 << 20)     /* R/W */
#define MSDC_CFG_VDDPD                  (0x1 << 21)     /* R/W */
#define MSDC_CFG_PRCFG0                 (0x3 << 22)     /* R/W */
#define MSDC_CFG_PRCFG1                 (0x3 << 24)     /* R/W */
#define MSDC_CFG_PRCFG2                 (0x3 << 26)     /* R/W */
#define MSDC_CFG_FIFOTHD                (0xfUL << 28)   /* R/W */

/* Masks for MSDC_STA */
#define MSDC_STA_BF                     (0x1 << 0)      /* RO */
#define MSDC_STA_BE                     (0x1 << 1)      /* RO */
#define MSDC_STA_DRQ                    (0x1 << 2)      /* RO */
#define MSDC_STA_INT                    (0x1 << 3)      /* RO */
#define MSDC_STA_FIFOCNT                (0xf << 4)      /* RO */
#define MSDC_STA_FIFOCLR                (0x1 << 14)     /* W */
#define MSDC_STA_BUSY                   (0x1 << 15)     /* R */

/* Masks for MSDC_INT */
#define MSDC_INT_DIRQ                   (0x1 << 0)      /* R/C */
#define MSDC_INT_PINIRQ                 (0x1 << 1)      /* R/C */
#define MSDC_INT_SDCMDIRQ               (0x1 << 2)      /* R/C */
#define MSDC_INT_SDDATIRQ               (0x1 << 3)      /* R/C */
#define MSDC_INT_SDMCIRQ                (0x1 << 4)      /* R/C */
#define MSDC_INT_MSIFIRQ                (0x1 << 5)      /* R/C */
#define MSDC_INT_SDR1BIRQ               (0x1 << 6)      /* R/C */
#define MSDC_INT_SDIOIRQ                (0x1 << 7)      /* R/C */

/* Masks for MSDC_PS */
#define MSDC_PS_CDEN                    (0x1 << 0)      /* R/W */
#define MSDC_PS_PIEN0                   (0x1 << 1)      /* R/W */
#define MSDC_PS_POEN0                   (0x1 << 2)      /* R/W */
#define MSDC_PS_PIN0                    (0x1 << 3)      /* R */
#define MSDC_PS_PINCH                   (0x1 << 4)      /* R/C */
#define MSDC_PS_DEBOUNCE                (0xf << 12)     /* R/W */
#define MSDC_PS_DAT                     (0xff << 16)    /* RO */
#define MSDC_PS_CMD                     (0x1 << 24)     /* RO */

/* Masks for MSDC_IOCON */
#define MSDC_IOCON_ODCCFG0              (0x7 << 0)      /* R/W */
#define MSDC_IOCON_ODCCFG1              (0x7 << 3)      /* R/W */
#define MSDC_IOCON_SRCFG0               (0x1 << 6)      /* R/W */
#define MSDC_IOCON_SRCFG1               (0x1 << 7)      /* R/W */
#define MSDC_IOCON_PRCFG3               (0x3 << 8)      /* R/W */
#define MSDC_IOCON_CMDRE                (0x1 << 15)     /* R/W */
#define MSDC_IOCON_DSW                  (0x1 << 16)     /* R/W */
#define MSDC_IOCON_INTLH                (0x3 << 17)     /* R/W */
#define MSDC_IOCON_CMDSEL               (0x1 << 19)     /* R/W */
#define MSDC_IOCON_CRCDIS               (0x1 << 20)     /* R/W */
#define MSDC_IOCON_DLT                  (0xffUL << 24)  /* R/W */

/* Masks for SDC_CFG */
#define SDC_CFG_BLKLEN                  (0xfff << 0)    /* R/W */
#define SDC_CFG_BSYDLY                  (0xf << 12)     /* R/W */
#define SDC_CFG_SIEN                    (0x1 << 16)     /* R/W */
#define SDC_CFG_MDLEN                   (0x1 << 17)     /* R/W */
#define SDC_CFG_MDLEN8                  (0x1 << 18)     /* R/W */
#define SDC_CFG_SDIO                    (0x1 << 19)     /* R/W */
#define SDC_CFG_WDOD                    (0xf << 20)     /* R/W */
#define SDC_CFG_DTOC                    (0xffUL << 24)  /* R/W */

/* Masks for SDC_CMD */
#define SDC_CMD_CMD                     (0x3f << 0)     /* R/W */
#define SDC_CMD_BREAK                   (0x1 << 6)      /* R/W */
#define SDC_CMD_RSPTYP                  (0x7 << 7)      /* R/W */
#define SDC_CMD_IDRT                    (0x1 << 10)     /* R/W */
#define SDC_CMD_DTYPE                   (0x3 << 11)     /* R/W */
#define SDC_CMD_RW                      (0x1 << 13)     /* R/W */
#define SDC_CMD_STOP                    (0x1 << 14)     /* R/W */
#define SDC_CMD_INTC                    (0x1 << 15)	    /* R/W */
#define SDC_CMD_CMDFAIL                 (0x1 << 16)     /* R/W */

/* Masks for SDC_STA */
#define SDC_STA_BESDCBUSY               (0x1 << 0)      /* RO */
#define SDC_STA_BECMDBUSY               (0x1 << 1)      /* RO */
#define SDC_STA_BEDATBUSY               (0x1 << 2)      /* RO */
#define SDC_STA_FECMDBUSY               (0x1 << 3)      /* RO */
#define SDC_STA_FEDATBUSY               (0x1 << 4)      /* RO */
#define SDC_STA_WP                      (0x1 << 15)     /* R */

/* Masks for SDC_CMDSTA */
#define SDC_CMDSTA_CMDRDY               (0x1 << 0)      /* R/C */
#define SDC_CMDSTA_CMDTO                (0x1 << 1)      /* R/C */
#define SDC_CMDSTA_RSPCRCERR            (0x1 << 2)      /* R/C */
#define SDC_CMDSTA_MMCIRQ               (0x1 << 3)      /* R/C */

/* Masks for SDC_DATSTA */
#define SDC_DATSTA_BLKDONE              (0x1 << 0)      /* R/C */
#define SDC_DATSTA_DATTO                (0x1 << 1)      /* R/C */
#define SDC_DATSTA_DATCRCERR            (0x1 << 2)      /* R/C */

/* Masks for SDC_CSTA */
#define SDC_CSTA_AKE_SEQ_ERR            (0x1 << 3)
#define SDC_CSTA_CID_CSD_OVW            (0x1 << 16)
#define SDC_CSTA_OVERRUN                (0x1 << 17)
#define SDC_CSTA_UNDERRUN               (0x1 << 18)
#define SDC_CSTA_ERROR                  (0x1 << 19)
#define SDC_CSTA_CC_ERR                 (0x1 << 20)
#define SDC_CSTA_CARD_ECC_FAILED        (0x1 << 21)
#define SDC_CSTA_ILLEGAL_CMD            (0x1 << 22)
#define SDC_CSTA_COM_CRC_ERR            (0x1 << 23)
#define SDC_CSTA_LK_ULK_FAILED          (0x1 << 24)
#define SDC_CSTA_WP_VIOLATION           (0x1 << 26)
#define SDC_CSTA_ERASE_PARM             (0x1 << 27)
#define SDC_CSTA_ERASE_SEQ_ERR          (0x1 << 28)
#define SDC_CSTA_BLK_LEN_ERR            (0x1 << 29)
#define SDC_CSTA_ADDR_ERR               (0x1 << 30)
#define SDC_CSTA_OUT_OF_RANGE           (0x1 << 31)

/* Masks for SDC_IRQMASK0 */
#define SDC_IRQMASK0_CMDRDY             (0x1 << 0)
#define SDC_IRQMASK0_CMDTO              (0x1 << 1)
#define SDC_IRQMASK0_RSPCRCERR          (0x1 << 2)
#define SDC_IRQMASK0_MMCIRQ             (0x1 << 3)
#define SDC_IRQMASK0_BLKDONE            (0x1 << 16)
#define SDC_IRQMASK0_DATTO              (0x1 << 17)
#define SDC_IRQMASK0_DATACRCERR         (0x1 << 18)

/* Masks for SDC_IRQMASK1 */
#define SDC_IRQMASK1_AKE_SEQ_ERR        (0x1 << 3)
#define SDC_IRQMASK1_CID_CSD_OVW        (0x1 << 16)
#define SDC_IRQMASK1_OVERRUN            (0x1 << 17)
#define SDC_IRQMASK1_UNDERRUN           (0x1 << 18)
#define SDC_IRQMASK1_ERROR              (0x1 << 19)
#define SDC_IRQMASK1_CC_ERR             (0x1 << 20)
#define SDC_IRQMASK1_CARD_ECC_FAILED    (0x1 << 21)
#define SDC_IRQMASK1_ILLEGAL_CMD        (0x1 << 22)
#define SDC_IRQMASK1_COM_CRC_ERR        (0x1 << 23)
#define SDC_IRQMASK1_LK_ULK_FAILED      (0x1 << 24)
#define SDC_IRQMASK1_WP_VIOLATION       (0x1 << 26)
#define SDC_IRQMASK1_ERASE_PARM         (0x1 << 27)
#define SDC_IRQMASK1_ERASE_SEQ_ERR      (0x1 << 28)
#define SDC_IRQMASK1_BLK_LEN_ERR        (0x1 << 29)
#define SDC_IRQMASK1_ADDR_ERR           (0x1 << 30)
#define SDC_IRQMASK1_OUT_OF_RANGE       (0x1 << 31)

/* Masks for SDIO_CFG */
#define SDIO_CFG_INTEN                  (0x1 << 0)  /* R/W */
#define SDIO_CFG_INTSEL                 (0x1 << 1)  /* R/W */
#define SDIO_CFG_DSBSEL                 (0x1 << 2)  /* R/W */

/* Masks for SDIO_STA */
#define SDIO_STA_IRQ                    (0x1 << 0)  /* RO */

/* configure the output driving capacity and slew rate */
#define MSDC_ODC_4MA                    (0x0)
#define MSDC_ODC_8MA                    (0x2)
#define MSDC_ODC_12MA                   (0x4)
#define MSDC_ODC_16MA                   (0x6)

#define MSDC_ODC_SLEW_FAST              (0)
#define MSDC_ODC_SLEW_SLOW              (1)

#define MSDC_DSW_NODELAY                (0)
#define MSDC_DSW_DELAY                  (1)

enum {
    PIN_PULL_NONE = 0,
    PIN_PULL_DOWN = 1,
    PIN_PULL_UP   = 2,
    PIN_PULL_KEEP = 3
};

enum {
    RESP_NONE = 0,
    RESP_R1   = 1,
    RESP_R2   = 2,
    RESP_R3   = 3,
    RESP_R4   = 4,
    RESP_R5   = 5,
    RESP_R6   = 6,
    RESP_R1B  = 7
};

typedef struct {
    u32 msdc:1;
    u32 pad1:1;
    u32 nocrc:1;
    u32 rst:1;
    u32 clksrc:1;
    u32 stdby:1;
    u32 red:1;
    u32 sclkon:1;
    u32 sclkf:8;
    u32 inten:1;
    u32 dmaen:1;
    u32 pinen:1;
    u32 dirqen:1;
    u32 rcden:1;
    u32 vddpd:1;
    u32 prcfg0:2;
    u32 prcfg1:2;
    u32 prcfg2:2;
    u32 fifothd:4;		
} msdc_cfg_reg;
typedef struct {
    u32 bf:1;
    u32 be:1;
    u32 drq:1;
    u32 intr:1;
    u32 fifocnt:4;
    u32 pad1:6;
    u32 fifoclr:1;
    u32 busy:1;
    u32 pad2:16;
} msdc_sta_reg;
typedef struct {
    u32 dirq:1;
    u32 pinirq:1;
    u32 sdcmdirq:1;
    u32 sddatirq:1;
    u32 sdmcirq:1;
    u32 msifirq:1;
    u32 sdr1b:1;
    u32 sdioirq:1;
    u32 pad:24;
} msdc_int_reg;
typedef struct {
    u32 val;
} msdc_dat_reg;
typedef struct {
    u32 cden:1;
    u32 pien0:1;
    u32 poen0:1;
    u32 pin0:1;
    u32 pinchg:1;
    u32 pad1:7;
    u32 cddebounce:4;
    u32 dat:8;
    u32 cmd:1;
    u32 pad2:7;
} msdc_ps_reg;
typedef struct {
    u32 odccfg0:3;
    u32 odccfg1:3;
    u32 srcfg0:1;
    u32 srcfg1:1;
    u32 prcfg3:2;
    u32 pad2:5;
    u32 cmdre:1;
    u32 dsw:1;
    u32 intlh:2;
    u32 cmdsel:1;
    u32 crcdis:1;
    u32 pad3:3;
    u32 dlt:8;
} msdc_iocon_reg;
typedef struct {
    u32 blklen:12;
    u32 busydly:4;
    u32 sien:1;
    u32 mdlen:1;
    u32 mdlw8:1;
    u32 sdio:1;
    u32 wdod:4;
    u32 dtoc:8;
} sdc_cfg_reg;
typedef struct {
    u32 cmd:6;
    u32 brk:1;
    u32 rsptyp:3;
    u32 idrt:1;
    u32 dtype:2;
    u32 rw:1;
    u32 stop:1;
    u32 intc:1;
    u32 cmdfail:1;
    u32 pad:15;
} sdc_cmd_reg;
typedef struct {
    u32 arg;
} sdc_arg_reg;
typedef struct {
    u32 besdcbusy:1;
    u32 becmdbusy:1;
    u32 bedatbusy:1;
    u32 fecmdbusy:1;
    u32 fedatbusy:1;
    u32 pad1:10;
    u32 wp:1;
    u32 pad2:16;
} sdc_sta_reg;
typedef struct {
    u32 val;
} sdc_resp0_reg;
typedef struct {
    u32 val;	
} sdc_resp1_reg;
typedef struct {
    u32 val;	
} sdc_resp2_reg;
typedef struct {
    u32 val;	
} sdc_resp3_reg;
typedef struct {
    u32 cmdrdy:1;
    u32 cmtto:1;
    u32 rspcrcerr:1;
    u32 mmcirq:1;
    u32 pad:28;
} sdc_cmdsta_reg;
typedef struct {
    u32 blkdone:1;
    u32 datto:1;
    u32 datcrcerr:1;
    u32 pad:29;
} sdc_datsta_reg;
typedef struct {
    u32 csta;
} sdc_csta_reg;
typedef struct {
    u32 cmdrdy:1;
    u32 cmdto:1;
    u32 rspcrerr:1;
    u32 mmcirq:1;
    u32 pad1:12;
    u32 blkdone:1;
    u32 datto:1;
    u32 datcrcerr:1;
    u32 pad2:13;
} sdc_irqmask0_reg;
typedef struct {
    u32 pad1:3;
    u32 ake_seq_err:1;
    u32 pad2:12;
    u32 cid_csd_ovw:1;
    u32 overrun:1;
    u32 underrun:1;
    u32 error:1;
    u32 cc_err:1;
    u32 card_ecc_failed:1;
    u32 ill_cmd:1;
    u32 com_crc_err:1;
    u32 lk_ulk_failed:1;
    u32 pad3:1;
    u32 wp_violation:1;
    u32 erase_parm:1;
    u32 erase_seq_err:1;
    u32 blk_len_err:1;
    u32 addr_err:1;
    u32 out_of_range:1;
} sdc_irqmask1_reg;
typedef struct {
    u32 inten:1;
    u32 intsel:1;
    u32 dsbsel:1;
    u32 reserved:29;
} sdio_cfg_reg;
typedef struct {
    u32 irq:1;
    u32 reserved:31;
} sdio_sta_reg;

struct mt6516_sd_regs {
    msdc_cfg_reg   msdc_cfg;       /* base+0x00h */
    msdc_sta_reg   msdc_sta;       /* base+0x04h */
    msdc_int_reg   msdc_int;       /* base+0x08h */
    msdc_dat_reg   msdc_dat;       /* base+0x0Ch */
    msdc_ps_reg    msdc_ps;        /* base+0x10h */
    msdc_iocon_reg msdc_iocon;     /* base+0x14h */
    u32             reserved[2];
    sdc_cfg_reg    sdc_cfg;        /* base+0x20h */
    sdc_cmd_reg    sdc_cmd;        /* base+0x24h */
    sdc_arg_reg    sdc_arg;        /* base+0x28h */
    sdc_sta_reg    sdc_sta;        /* base+0x2Ch */
    sdc_resp0_reg  sdc_resp0;      /* base+0x30h */
    sdc_resp1_reg  sdc_resp1;      /* base+0x34h */
    sdc_resp2_reg  sdc_resp2;      /* base+0x38h */
    sdc_resp3_reg  sdc_resp3;      /* base+0x3Ch */
    sdc_cmdsta_reg sdc_cmdsta;     /* base+0x40h */
    sdc_datsta_reg sdc_datsta;     /* base+0x44h */
    sdc_csta_reg   sdc_csta;       /* base+0x48h */
    sdc_irqmask0_reg sdc_irqmask0; /* base+0x4Ch */
    sdc_irqmask1_reg sdc_irqmask1; /* base+0x50h */
    sdio_cfg_reg     sdio_cfg;     /* base+0x54h */
    sdio_sta_reg     sdio_sta;     /* base+0x58h */
};

struct mt6516_sd_host
{
    struct mt6516_sd_host_hw    *hw;

    struct mmc_host             *mmc;           /* mmc structure */
    struct mmc_command          *cmd;
    struct mmc_data             *data;
    int                         cmd_rsp;
    int                         cmd_rsp_done;
    int                         cmd_r1b_done;

    spinlock_t                  lock;           /* mutex */

    u32                         base;           /* host base address */
    int                         id;             /* host id */

    struct scatterlist          *cur_sg;        /* current sg entry */
    unsigned int                num_sg;         /* number of entries left */
    u32                         xfer_size;      /* total transferred size */

    struct mt_dma_conf          *dma;           /* dma channel */
    int                         dma_dir;        /* dma transfer direction */
    u32                         dma_addr;       /* dma transfer address */
    u32                         dma_left_size;  /* dma transfer left size */
    u32                         dma_xfer_size;  /* dma transfer size in bytes */
    int                         dma_xfer;       /* dma transfer mode */

    u32                         timeout_ns;     /* data timeout ns */
    u32                         timeout_clks;   /* data timeout clks */

    atomic_t                    abort;          /* abort transfer */

    int                         sd_irq;         /* host interrupt */
    int                         cd_irq;         /* card detect interrupt */

    struct tasklet_struct       card_tasklet;
    struct tasklet_struct       fifo_tasklet;
    struct tasklet_struct       dma_tasklet;

    struct completion           cmd_done;
    struct completion           xfer_done;
    struct pm_message           pm_state;

    u32                         mclk;           /* mmc subsystem clock */
    u32                         hclk;           /* host clock speed */		
    u32                         sclk;           /* SD/MS clock speed */
    u8                          card_clkon;     /* Card clock on ? */
    u8                          core_power;     /* core power */
    u8                          power_mode;     /* host power mode */
    u8                          card_inserted;  /* card inserted ? */
    u8                          suspend;        /* host suspended ? */
    u8                          reserved[3];
    u64                         starttime;
};

static inline unsigned int uffs(unsigned int x)
{
    unsigned int r = 1;

    if (!x)
        return 0;
    if (!(x & 0xffff)) {
        x >>= 16;
        r += 16;
    }
    if (!(x & 0xff)) {
        x >>= 8;
        r += 8;
    }
    if (!(x & 0xf)) {
        x >>= 4;
        r += 4;
    }
    if (!(x & 3)) {
        x >>= 2;
        r += 2;
    }
    if (!(x & 1)) {
        x >>= 1;
        r += 1;
    }
    return r;
}

#define sdr_read16(reg)          __raw_readw(reg)
#define sdr_read32(reg)          __raw_readl(reg)
#define sdr_write16(reg,val)     __raw_writew(val,reg)
#define sdr_write32(reg,val)     __raw_writel(val,reg)

#define sdr_set_bits(reg,bs)     ((*(volatile u32*)(reg)) |= (u32)(bs))
#define sdr_clr_bits(reg,bs)     ((*(volatile u32*)(reg)) &= ~((u32)(bs)))

#define sdr_set_field(reg,field,val) \
    do {	\
        volatile unsigned int tv = sdr_read32(reg);	\
        tv &= ~(field); \
        tv |= ((val) << (uffs((unsigned int)field) - 1)); \
        sdr_write32(reg,tv); \
    } while(0)
#define sdr_get_field(reg,field,val) \
    do {	\
        volatile unsigned int tv = sdr_read32(reg);	\
        val = ((tv & (field)) >> (uffs((unsigned int)field) - 1)); \
    } while(0)

#endif

