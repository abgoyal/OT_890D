
#ifndef __IBM_NEWEMAC_CORE_H
#define __IBM_NEWEMAC_CORE_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/of_platform.h>

#include <asm/io.h>
#include <asm/dcr.h>

#include "emac.h"
#include "phy.h"
#include "zmii.h"
#include "rgmii.h"
#include "mal.h"
#include "tah.h"
#include "debug.h"

#define NUM_TX_BUFF			CONFIG_IBM_NEW_EMAC_TXB
#define NUM_RX_BUFF			CONFIG_IBM_NEW_EMAC_RXB

/* Simple sanity check */
#if NUM_TX_BUFF > 256 || NUM_RX_BUFF > 256
#error Invalid number of buffer descriptors (greater than 256)
#endif

#define EMAC_MIN_MTU			46

/* Maximum L2 header length (VLAN tagged, no FCS) */
#define EMAC_MTU_OVERHEAD		(6 * 2 + 2 + 4)

/* RX BD size for the given MTU */
static inline int emac_rx_size(int mtu)
{
	if (mtu > ETH_DATA_LEN)
		return MAL_MAX_RX_SIZE;
	else
		return mal_rx_size(ETH_DATA_LEN + EMAC_MTU_OVERHEAD);
}

#define EMAC_DMA_ALIGN(x)		ALIGN((x), dma_get_cache_alignment())

#define EMAC_RX_SKB_HEADROOM		\
	EMAC_DMA_ALIGN(CONFIG_IBM_NEW_EMAC_RX_SKB_HEADROOM)

/* Size of RX skb for the given MTU */
static inline int emac_rx_skb_size(int mtu)
{
	int size = max(mtu + EMAC_MTU_OVERHEAD, emac_rx_size(mtu));
	return EMAC_DMA_ALIGN(size + 2) + EMAC_RX_SKB_HEADROOM;
}

/* RX DMA sync size */
static inline int emac_rx_sync_size(int mtu)
{
	return EMAC_DMA_ALIGN(emac_rx_size(mtu) + 2);
}


/* Normal TX/RX Statistics */
struct emac_stats {
	u64 rx_packets;
	u64 rx_bytes;
	u64 tx_packets;
	u64 tx_bytes;
	u64 rx_packets_csum;
	u64 tx_packets_csum;
};

/* Error statistics */
struct emac_error_stats {
	u64 tx_undo;

	/* Software RX Errors */
	u64 rx_dropped_stack;
	u64 rx_dropped_oom;
	u64 rx_dropped_error;
	u64 rx_dropped_resize;
	u64 rx_dropped_mtu;
	u64 rx_stopped;
	/* BD reported RX errors */
	u64 rx_bd_errors;
	u64 rx_bd_overrun;
	u64 rx_bd_bad_packet;
	u64 rx_bd_runt_packet;
	u64 rx_bd_short_event;
	u64 rx_bd_alignment_error;
	u64 rx_bd_bad_fcs;
	u64 rx_bd_packet_too_long;
	u64 rx_bd_out_of_range;
	u64 rx_bd_in_range;
	/* EMAC IRQ reported RX errors */
	u64 rx_parity;
	u64 rx_fifo_overrun;
	u64 rx_overrun;
	u64 rx_bad_packet;
	u64 rx_runt_packet;
	u64 rx_short_event;
	u64 rx_alignment_error;
	u64 rx_bad_fcs;
	u64 rx_packet_too_long;
	u64 rx_out_of_range;
	u64 rx_in_range;

	/* Software TX Errors */
	u64 tx_dropped;
	/* BD reported TX errors */
	u64 tx_bd_errors;
	u64 tx_bd_bad_fcs;
	u64 tx_bd_carrier_loss;
	u64 tx_bd_excessive_deferral;
	u64 tx_bd_excessive_collisions;
	u64 tx_bd_late_collision;
	u64 tx_bd_multple_collisions;
	u64 tx_bd_single_collision;
	u64 tx_bd_underrun;
	u64 tx_bd_sqe;
	/* EMAC IRQ reported TX errors */
	u64 tx_parity;
	u64 tx_underrun;
	u64 tx_sqe;
	u64 tx_errors;
};

#define EMAC_ETHTOOL_STATS_COUNT	((sizeof(struct emac_stats) + \
					  sizeof(struct emac_error_stats)) \
					 / sizeof(u64))

struct emac_instance {
	struct net_device		*ndev;
	struct resource			rsrc_regs;
	struct emac_regs		__iomem *emacp;
	struct of_device		*ofdev;
	struct device_node		**blist; /* bootlist entry */

	/* MAL linkage */
	u32				mal_ph;
	struct of_device		*mal_dev;
	u32				mal_rx_chan;
	u32				mal_tx_chan;
	struct mal_instance		*mal;
	struct mal_commac		commac;

	/* PHY infos */
	u32				phy_mode;
	u32				phy_map;
	u32				phy_address;
	u32				phy_feat_exc;
	struct mii_phy			phy;
	struct mutex			link_lock;
	struct delayed_work		link_work;
	int				link_polling;

	/* GPCS PHY infos */
	u32				gpcs_address;

	/* Shared MDIO if any */
	u32				mdio_ph;
	struct of_device		*mdio_dev;
	struct emac_instance		*mdio_instance;
	struct mutex			mdio_lock;

	/* ZMII infos if any */
	u32				zmii_ph;
	u32				zmii_port;
	struct of_device		*zmii_dev;

	/* RGMII infos if any */
	u32				rgmii_ph;
	u32				rgmii_port;
	struct of_device		*rgmii_dev;

	/* TAH infos if any */
	u32				tah_ph;
	u32				tah_port;
	struct of_device		*tah_dev;

	/* IRQs */
	int				wol_irq;
	int				emac_irq;

	/* OPB bus frequency in Mhz */
	u32				opb_bus_freq;

	/* Cell index within an ASIC (for clk mgmnt) */
	u32				cell_index;

	/* Max supported MTU */
	u32				max_mtu;

	/* Feature bits (from probe table) */
	unsigned int			features;

	/* Tx and Rx fifo sizes & other infos in bytes */
	u32				tx_fifo_size;
	u32				tx_fifo_size_gige;
	u32				rx_fifo_size;
	u32				rx_fifo_size_gige;
	u32				fifo_entry_size;
	u32				mal_burst_size; /* move to MAL ? */

	/* IAHT and GAHT filter parameterization */
	u32				xaht_slots_shift;
	u32				xaht_width_shift;

	/* Descriptor management
	 */
	struct mal_descriptor		*tx_desc;
	int				tx_cnt;
	int				tx_slot;
	int				ack_slot;

	struct mal_descriptor		*rx_desc;
	int				rx_slot;
	struct sk_buff			*rx_sg_skb;	/* 1 */
	int 				rx_skb_size;
	int				rx_sync_size;

	struct sk_buff			*tx_skb[NUM_TX_BUFF];
	struct sk_buff			*rx_skb[NUM_RX_BUFF];

	/* Stats
	 */
	struct emac_error_stats		estats;
	struct net_device_stats		nstats;
	struct emac_stats 		stats;

	/* Misc
	 */
	int				reset_failed;
	int				stop_timeout;	/* in us */
	int				no_mcast;
	int				mcast_pending;
	int				opened;
	struct work_struct		reset_work;
	spinlock_t			lock;
};


#define EMAC_FTR_NO_FLOW_CONTROL_40x	0x00000001
#define EMAC_FTR_EMAC4			0x00000002
#define EMAC_FTR_STACR_OC_INVERT	0x00000004
#define EMAC_FTR_HAS_TAH		0x00000008
#define EMAC_FTR_HAS_ZMII		0x00000010
#define EMAC_FTR_HAS_RGMII		0x00000020
#define EMAC_FTR_HAS_NEW_STACR		0x00000040
#define EMAC_FTR_440GX_PHY_CLK_FIX	0x00000080
#define EMAC_FTR_440EP_PHY_CLK_FIX	0x00000100
#define EMAC_FTR_EMAC4SYNC		0x00000200
#define EMAC_FTR_460EX_PHY_CLK_FIX	0x00000400


enum {
	EMAC_FTRS_ALWAYS	= 0,

	EMAC_FTRS_POSSIBLE	=
#ifdef CONFIG_IBM_NEW_EMAC_EMAC4
	    EMAC_FTR_EMAC4	| EMAC_FTR_EMAC4SYNC	|
	    EMAC_FTR_HAS_NEW_STACR	|
	    EMAC_FTR_STACR_OC_INVERT | EMAC_FTR_440GX_PHY_CLK_FIX |
#endif
#ifdef CONFIG_IBM_NEW_EMAC_TAH
	    EMAC_FTR_HAS_TAH	|
#endif
#ifdef CONFIG_IBM_NEW_EMAC_ZMII
	    EMAC_FTR_HAS_ZMII	|
#endif
#ifdef CONFIG_IBM_NEW_EMAC_RGMII
	    EMAC_FTR_HAS_RGMII	|
#endif
#ifdef CONFIG_IBM_NEW_EMAC_NO_FLOW_CTRL
	    EMAC_FTR_NO_FLOW_CONTROL_40x |
#endif
	EMAC_FTR_460EX_PHY_CLK_FIX |
	EMAC_FTR_440EP_PHY_CLK_FIX,
};

static inline int emac_has_feature(struct emac_instance *dev,
				   unsigned long feature)
{
	return (EMAC_FTRS_ALWAYS & feature) ||
	       (EMAC_FTRS_POSSIBLE & dev->features & feature);
}


#define	EMAC4_XAHT_SLOTS_SHIFT		6
#define	EMAC4_XAHT_WIDTH_SHIFT		4

#define	EMAC4SYNC_XAHT_SLOTS_SHIFT	8
#define	EMAC4SYNC_XAHT_WIDTH_SHIFT	5

#define	EMAC_XAHT_SLOTS(dev)         	(1 << (dev)->xaht_slots_shift)
#define	EMAC_XAHT_WIDTH(dev)         	(1 << (dev)->xaht_width_shift)
#define	EMAC_XAHT_REGS(dev)          	(1 << ((dev)->xaht_slots_shift - \
					       (dev)->xaht_width_shift))

#define	EMAC_XAHT_CRC_TO_SLOT(dev, crc)			\
	((EMAC_XAHT_SLOTS(dev) - 1) -			\
	 ((crc) >> ((sizeof (u32) * BITS_PER_BYTE) -	\
		    (dev)->xaht_slots_shift)))

#define	EMAC_XAHT_SLOT_TO_REG(dev, slot)		\
	((slot) >> (dev)->xaht_width_shift)

#define	EMAC_XAHT_SLOT_TO_MASK(dev, slot)		\
	((u32)(1 << (EMAC_XAHT_WIDTH(dev) - 1)) >>	\
	 ((slot) & (u32)(EMAC_XAHT_WIDTH(dev) - 1)))

static inline u32 *emac_xaht_base(struct emac_instance *dev)
{
	struct emac_regs __iomem *p = dev->emacp;
	int offset;

	/* The first IAHT entry always is the base of the block of
	 * IAHT and GAHT registers.
	 */
	if (emac_has_feature(dev, EMAC_FTR_EMAC4SYNC))
		offset = offsetof(struct emac_regs, u1.emac4sync.iaht1);
	else
		offset = offsetof(struct emac_regs, u0.emac4.iaht1);

	return ((u32 *)((ptrdiff_t)p + offset));
}

static inline u32 *emac_gaht_base(struct emac_instance *dev)
{
	/* GAHT registers always come after an identical number of
	 * IAHT registers.
	 */
	return (emac_xaht_base(dev) + EMAC_XAHT_REGS(dev));
}

static inline u32 *emac_iaht_base(struct emac_instance *dev)
{
	/* IAHT registers always come before an identical number of
	 * GAHT registers.
	 */
	return (emac_xaht_base(dev));
}

#define EMAC_ETHTOOL_REGS_ZMII		0x00000001
#define EMAC_ETHTOOL_REGS_RGMII		0x00000002
#define EMAC_ETHTOOL_REGS_TAH		0x00000004

struct emac_ethtool_regs_hdr {
	u32 components;
};

struct emac_ethtool_regs_subhdr {
	u32 version;
	u32 index;
};

#define EMAC_ETHTOOL_REGS_VER		0
#define EMAC_ETHTOOL_REGS_SIZE(dev) 	((dev)->rsrc_regs.end - \
					 (dev)->rsrc_regs.start + 1)
#define EMAC4_ETHTOOL_REGS_VER      	1
#define EMAC4_ETHTOOL_REGS_SIZE(dev)	((dev)->rsrc_regs.end -	\
					 (dev)->rsrc_regs.start + 1)

#endif /* __IBM_NEWEMAC_CORE_H */
