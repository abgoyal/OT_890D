
/* typhoon.h:	chip info for the 3Com 3CR990 family of controllers */


struct basic_ring {
	u8 *ringBase;
	u32 lastWrite;
};

struct transmit_ring {
	u8 *ringBase;
	u32 lastWrite;
	u32 lastRead;
	int writeRegister;
};

struct typhoon_indexes {
	/* The first four are written by the host, and read by the NIC */
	volatile __le32 rxHiCleared;
	volatile __le32 rxLoCleared;
	volatile __le32 rxBuffReady;
	volatile __le32 respCleared;

	/* The remaining are written by the NIC, and read by the host */
	volatile __le32 txLoCleared;
	volatile __le32 txHiCleared;
	volatile __le32 rxLoReady;
	volatile __le32 rxBuffCleared;
	volatile __le32 cmdCleared;
	volatile __le32 respReady;
	volatile __le32 rxHiReady;
} __attribute__ ((packed));

struct typhoon_interface {
	__le32 ringIndex;
	__le32 ringIndexHi;
	__le32 txLoAddr;
	__le32 txLoAddrHi;
	__le32 txLoSize;
	__le32 txHiAddr;
	__le32 txHiAddrHi;
	__le32 txHiSize;
	__le32 rxLoAddr;
	__le32 rxLoAddrHi;
	__le32 rxLoSize;
	__le32 rxBuffAddr;
	__le32 rxBuffAddrHi;
	__le32 rxBuffSize;
	__le32 cmdAddr;
	__le32 cmdAddrHi;
	__le32 cmdSize;
	__le32 respAddr;
	__le32 respAddrHi;
	__le32 respSize;
	__le32 zeroAddr;
	__le32 zeroAddrHi;
	__le32 rxHiAddr;
	__le32 rxHiAddrHi;
	__le32 rxHiSize;
} __attribute__ ((packed));

struct tx_desc {
	u8  flags;
#define TYPHOON_TYPE_MASK	0x07
#define 	TYPHOON_FRAG_DESC	0x00
#define 	TYPHOON_TX_DESC		0x01
#define 	TYPHOON_CMD_DESC	0x02
#define 	TYPHOON_OPT_DESC	0x03
#define 	TYPHOON_RX_DESC		0x04
#define 	TYPHOON_RESP_DESC	0x05
#define TYPHOON_OPT_TYPE_MASK	0xf0
#define 	TYPHOON_OPT_IPSEC	0x00
#define 	TYPHOON_OPT_TCP_SEG	0x10
#define TYPHOON_CMD_RESPOND	0x40
#define TYPHOON_RESP_ERROR	0x40
#define TYPHOON_RX_ERROR	0x40
#define TYPHOON_DESC_VALID	0x80
	u8  numDesc;
	__le16 len;
	union {
		struct {
			__le32 addr;
			__le32 addrHi;
		} frag;
		u64 tx_addr;	/* opaque for hardware, for TX_DESC */
	};
	__le32 processFlags;
#define TYPHOON_TX_PF_NO_CRC		__constant_cpu_to_le32(0x00000001)
#define TYPHOON_TX_PF_IP_CHKSUM		__constant_cpu_to_le32(0x00000002)
#define TYPHOON_TX_PF_TCP_CHKSUM	__constant_cpu_to_le32(0x00000004)
#define TYPHOON_TX_PF_TCP_SEGMENT	__constant_cpu_to_le32(0x00000008)
#define TYPHOON_TX_PF_INSERT_VLAN	__constant_cpu_to_le32(0x00000010)
#define TYPHOON_TX_PF_IPSEC		__constant_cpu_to_le32(0x00000020)
#define TYPHOON_TX_PF_VLAN_PRIORITY	__constant_cpu_to_le32(0x00000040)
#define TYPHOON_TX_PF_UDP_CHKSUM	__constant_cpu_to_le32(0x00000080)
#define TYPHOON_TX_PF_PAD_FRAME		__constant_cpu_to_le32(0x00000100)
#define TYPHOON_TX_PF_RESERVED		__constant_cpu_to_le32(0x00000e00)
#define TYPHOON_TX_PF_VLAN_MASK		__constant_cpu_to_le32(0x0ffff000)
#define TYPHOON_TX_PF_INTERNAL		__constant_cpu_to_le32(0xf0000000)
#define TYPHOON_TX_PF_VLAN_TAG_SHIFT	12
} __attribute__ ((packed));

struct tcpopt_desc {
	u8  flags;
	u8  numDesc;
	__le16 mss_flags;
#define TYPHOON_TSO_FIRST		__constant_cpu_to_le16(0x1000)
#define TYPHOON_TSO_LAST		__constant_cpu_to_le16(0x2000)
	__le32 respAddrLo;
	__le32 bytesTx;
	__le32 status;
} __attribute__ ((packed));

struct ipsec_desc {
	u8  flags;
	u8  numDesc;
	__le16 ipsecFlags;
#define TYPHOON_IPSEC_GEN_IV	__constant_cpu_to_le16(0x0000)
#define TYPHOON_IPSEC_USE_IV	__constant_cpu_to_le16(0x0001)
	__le32 sa1;
	__le32 sa2;
	__le32 reserved;
} __attribute__ ((packed));

struct rx_desc {
	u8  flags;
	u8  numDesc;
	__le16 frameLen;
	u32 addr;	/* opaque, comes from virtAddr */
	u32 addrHi;	/* opaque, comes from virtAddrHi */
	__le32 rxStatus;
#define TYPHOON_RX_ERR_INTERNAL		__constant_cpu_to_le32(0x00000000)
#define TYPHOON_RX_ERR_FIFO_UNDERRUN	__constant_cpu_to_le32(0x00000001)
#define TYPHOON_RX_ERR_BAD_SSD		__constant_cpu_to_le32(0x00000002)
#define TYPHOON_RX_ERR_RUNT		__constant_cpu_to_le32(0x00000003)
#define TYPHOON_RX_ERR_CRC		__constant_cpu_to_le32(0x00000004)
#define TYPHOON_RX_ERR_OVERSIZE		__constant_cpu_to_le32(0x00000005)
#define TYPHOON_RX_ERR_ALIGN		__constant_cpu_to_le32(0x00000006)
#define TYPHOON_RX_ERR_DRIBBLE		__constant_cpu_to_le32(0x00000007)
#define TYPHOON_RX_PROTO_MASK		__constant_cpu_to_le32(0x00000003)
#define TYPHOON_RX_PROTO_UNKNOWN	__constant_cpu_to_le32(0x00000000)
#define TYPHOON_RX_PROTO_IP		__constant_cpu_to_le32(0x00000001)
#define TYPHOON_RX_PROTO_IPX		__constant_cpu_to_le32(0x00000002)
#define TYPHOON_RX_VLAN			__constant_cpu_to_le32(0x00000004)
#define TYPHOON_RX_IP_FRAG		__constant_cpu_to_le32(0x00000008)
#define TYPHOON_RX_IPSEC		__constant_cpu_to_le32(0x00000010)
#define TYPHOON_RX_IP_CHK_FAIL		__constant_cpu_to_le32(0x00000020)
#define TYPHOON_RX_TCP_CHK_FAIL		__constant_cpu_to_le32(0x00000040)
#define TYPHOON_RX_UDP_CHK_FAIL		__constant_cpu_to_le32(0x00000080)
#define TYPHOON_RX_IP_CHK_GOOD		__constant_cpu_to_le32(0x00000100)
#define TYPHOON_RX_TCP_CHK_GOOD		__constant_cpu_to_le32(0x00000200)
#define TYPHOON_RX_UDP_CHK_GOOD		__constant_cpu_to_le32(0x00000400)
	__le16 filterResults;
#define TYPHOON_RX_FILTER_MASK		__constant_cpu_to_le16(0x7fff)
#define TYPHOON_RX_FILTERED		__constant_cpu_to_le16(0x8000)
	__le16 ipsecResults;
#define TYPHOON_RX_OUTER_AH_GOOD	__constant_cpu_to_le16(0x0001)
#define TYPHOON_RX_OUTER_ESP_GOOD	__constant_cpu_to_le16(0x0002)
#define TYPHOON_RX_INNER_AH_GOOD	__constant_cpu_to_le16(0x0004)
#define TYPHOON_RX_INNER_ESP_GOOD	__constant_cpu_to_le16(0x0008)
#define TYPHOON_RX_OUTER_AH_FAIL	__constant_cpu_to_le16(0x0010)
#define TYPHOON_RX_OUTER_ESP_FAIL	__constant_cpu_to_le16(0x0020)
#define TYPHOON_RX_INNER_AH_FAIL	__constant_cpu_to_le16(0x0040)
#define TYPHOON_RX_INNER_ESP_FAIL	__constant_cpu_to_le16(0x0080)
#define TYPHOON_RX_UNKNOWN_SA		__constant_cpu_to_le16(0x0100)
#define TYPHOON_RX_ESP_FORMAT_ERR	__constant_cpu_to_le16(0x0200)
	__be32 vlanTag;
} __attribute__ ((packed));

struct rx_free {
	__le32 physAddr;
	__le32 physAddrHi;
	u32 virtAddr;
	u32 virtAddrHi;
} __attribute__ ((packed));

struct cmd_desc {
	u8  flags;
	u8  numDesc;
	__le16 cmd;
#define TYPHOON_CMD_TX_ENABLE		__constant_cpu_to_le16(0x0001)
#define TYPHOON_CMD_TX_DISABLE		__constant_cpu_to_le16(0x0002)
#define TYPHOON_CMD_RX_ENABLE		__constant_cpu_to_le16(0x0003)
#define TYPHOON_CMD_RX_DISABLE		__constant_cpu_to_le16(0x0004)
#define TYPHOON_CMD_SET_RX_FILTER	__constant_cpu_to_le16(0x0005)
#define TYPHOON_CMD_READ_STATS		__constant_cpu_to_le16(0x0007)
#define TYPHOON_CMD_XCVR_SELECT		__constant_cpu_to_le16(0x0013)
#define TYPHOON_CMD_SET_MAX_PKT_SIZE	__constant_cpu_to_le16(0x001a)
#define TYPHOON_CMD_READ_MEDIA_STATUS	__constant_cpu_to_le16(0x001b)
#define TYPHOON_CMD_GOTO_SLEEP		__constant_cpu_to_le16(0x0023)
#define TYPHOON_CMD_SET_MULTICAST_HASH	__constant_cpu_to_le16(0x0025)
#define TYPHOON_CMD_SET_MAC_ADDRESS	__constant_cpu_to_le16(0x0026)
#define TYPHOON_CMD_READ_MAC_ADDRESS	__constant_cpu_to_le16(0x0027)
#define TYPHOON_CMD_VLAN_TYPE_WRITE	__constant_cpu_to_le16(0x002b)
#define TYPHOON_CMD_CREATE_SA		__constant_cpu_to_le16(0x0034)
#define TYPHOON_CMD_DELETE_SA		__constant_cpu_to_le16(0x0035)
#define TYPHOON_CMD_READ_VERSIONS	__constant_cpu_to_le16(0x0043)
#define TYPHOON_CMD_IRQ_COALESCE_CTRL	__constant_cpu_to_le16(0x0045)
#define TYPHOON_CMD_ENABLE_WAKE_EVENTS	__constant_cpu_to_le16(0x0049)
#define TYPHOON_CMD_SET_OFFLOAD_TASKS	__constant_cpu_to_le16(0x004f)
#define TYPHOON_CMD_HELLO_RESP		__constant_cpu_to_le16(0x0057)
#define TYPHOON_CMD_HALT		__constant_cpu_to_le16(0x005d)
#define TYPHOON_CMD_READ_IPSEC_INFO	__constant_cpu_to_le16(0x005e)
#define TYPHOON_CMD_GET_IPSEC_ENABLE	__constant_cpu_to_le16(0x0067)
#define TYPHOON_CMD_GET_CMD_LVL		__constant_cpu_to_le16(0x0069)
	u16 seqNo;
	__le16 parm1;
	__le32 parm2;
	__le32 parm3;
} __attribute__ ((packed));

struct resp_desc {
	u8  flags;
	u8  numDesc;
	__le16 cmd;
	__le16 seqNo;
	__le16 parm1;
	__le32 parm2;
	__le32 parm3;
} __attribute__ ((packed));

#define INIT_COMMAND_NO_RESPONSE(x, command)				\
	do { struct cmd_desc *_ptr = (x);				\
		memset(_ptr, 0, sizeof(struct cmd_desc));		\
		_ptr->flags = TYPHOON_CMD_DESC | TYPHOON_DESC_VALID;	\
		_ptr->cmd = command;					\
	} while(0)

/* We set seqNo to 1 if we're expecting a response from this command */
#define INIT_COMMAND_WITH_RESPONSE(x, command)				\
	do { struct cmd_desc *_ptr = (x);				\
		memset(_ptr, 0, sizeof(struct cmd_desc));		\
		_ptr->flags = TYPHOON_CMD_RESPOND | TYPHOON_CMD_DESC;	\
		_ptr->flags |= TYPHOON_DESC_VALID; 			\
		_ptr->cmd = command;					\
		_ptr->seqNo = 1;					\
	} while(0)

#define TYPHOON_RX_FILTER_DIRECTED	__constant_cpu_to_le16(0x0001)
#define TYPHOON_RX_FILTER_ALL_MCAST	__constant_cpu_to_le16(0x0002)
#define TYPHOON_RX_FILTER_BROADCAST	__constant_cpu_to_le16(0x0004)
#define TYPHOON_RX_FILTER_PROMISCOUS	__constant_cpu_to_le16(0x0008)
#define TYPHOON_RX_FILTER_MCAST_HASH	__constant_cpu_to_le16(0x0010)

struct stats_resp {
	u8  flags;
	u8  numDesc;
	__le16 cmd;
	__le16 seqNo;
	__le16 unused;
	__le32 txPackets;
	__le64 txBytes;
	__le32 txDeferred;
	__le32 txLateCollisions;
	__le32 txCollisions;
	__le32 txCarrierLost;
	__le32 txMultipleCollisions;
	__le32 txExcessiveCollisions;
	__le32 txFifoUnderruns;
	__le32 txMulticastTxOverflows;
	__le32 txFiltered;
	__le32 rxPacketsGood;
	__le64 rxBytesGood;
	__le32 rxFifoOverruns;
	__le32 BadSSD;
	__le32 rxCrcErrors;
	__le32 rxOversized;
	__le32 rxBroadcast;
	__le32 rxMulticast;
	__le32 rxOverflow;
	__le32 rxFiltered;
	__le32 linkStatus;
#define TYPHOON_LINK_STAT_MASK		__constant_cpu_to_le32(0x00000001)
#define TYPHOON_LINK_GOOD		__constant_cpu_to_le32(0x00000001)
#define TYPHOON_LINK_BAD		__constant_cpu_to_le32(0x00000000)
#define TYPHOON_LINK_SPEED_MASK		__constant_cpu_to_le32(0x00000002)
#define TYPHOON_LINK_100MBPS		__constant_cpu_to_le32(0x00000002)
#define TYPHOON_LINK_10MBPS		__constant_cpu_to_le32(0x00000000)
#define TYPHOON_LINK_DUPLEX_MASK	__constant_cpu_to_le32(0x00000004)
#define TYPHOON_LINK_FULL_DUPLEX	__constant_cpu_to_le32(0x00000004)
#define TYPHOON_LINK_HALF_DUPLEX	__constant_cpu_to_le32(0x00000000)
	__le32 unused2;
	__le32 unused3;
} __attribute__ ((packed));

#define TYPHOON_XCVR_10HALF	__constant_cpu_to_le16(0x0000)
#define TYPHOON_XCVR_10FULL	__constant_cpu_to_le16(0x0001)
#define TYPHOON_XCVR_100HALF	__constant_cpu_to_le16(0x0002)
#define TYPHOON_XCVR_100FULL	__constant_cpu_to_le16(0x0003)
#define TYPHOON_XCVR_AUTONEG	__constant_cpu_to_le16(0x0004)

#define TYPHOON_MEDIA_STAT_CRC_STRIP_DISABLE	__constant_cpu_to_le16(0x0004)
#define TYPHOON_MEDIA_STAT_COLLISION_DETECT	__constant_cpu_to_le16(0x0010)
#define TYPHOON_MEDIA_STAT_CARRIER_SENSE	__constant_cpu_to_le16(0x0020)
#define TYPHOON_MEDIA_STAT_POLARITY_REV		__constant_cpu_to_le16(0x0400)
#define TYPHOON_MEDIA_STAT_NO_LINK		__constant_cpu_to_le16(0x0800)

#define TYPHOON_MCAST_HASH_DISABLE	__constant_cpu_to_le16(0x0000)
#define TYPHOON_MCAST_HASH_ENABLE	__constant_cpu_to_le16(0x0001)
#define TYPHOON_MCAST_HASH_SET		__constant_cpu_to_le16(0x0002)

struct sa_descriptor {
	u8  flags;
	u8  numDesc;
	u16 cmd;
	u16 seqNo;
	u16 mode;
#define TYPHOON_SA_MODE_NULL		__constant_cpu_to_le16(0x0000)
#define TYPHOON_SA_MODE_AH		__constant_cpu_to_le16(0x0001)
#define TYPHOON_SA_MODE_ESP		__constant_cpu_to_le16(0x0002)
	u8  hashFlags;
#define TYPHOON_SA_HASH_ENABLE		0x01
#define TYPHOON_SA_HASH_SHA1		0x02
#define TYPHOON_SA_HASH_MD5		0x04
	u8  direction;
#define TYPHOON_SA_DIR_RX		0x00
#define TYPHOON_SA_DIR_TX		0x01
	u8  encryptionFlags;
#define TYPHOON_SA_ENCRYPT_ENABLE	0x01
#define TYPHOON_SA_ENCRYPT_DES		0x02
#define TYPHOON_SA_ENCRYPT_3DES		0x00
#define TYPHOON_SA_ENCRYPT_3DES_2KEY	0x00
#define TYPHOON_SA_ENCRYPT_3DES_3KEY	0x04
#define TYPHOON_SA_ENCRYPT_CBC		0x08
#define TYPHOON_SA_ENCRYPT_ECB		0x00
	u8  specifyIndex;
#define TYPHOON_SA_SPECIFY_INDEX	0x01
#define TYPHOON_SA_GENERATE_INDEX	0x00
	u32 SPI;
	u32 destAddr;
	u32 destMask;
	u8  integKey[20];
	u8  confKey[24];
	u32 index;
	u32 unused;
	u32 unused2;
} __attribute__ ((packed));

#define TYPHOON_OFFLOAD_TCP_CHKSUM	__constant_cpu_to_le32(0x00000002)
#define TYPHOON_OFFLOAD_UDP_CHKSUM	__constant_cpu_to_le32(0x00000004)
#define TYPHOON_OFFLOAD_IP_CHKSUM	__constant_cpu_to_le32(0x00000008)
#define TYPHOON_OFFLOAD_IPSEC		__constant_cpu_to_le32(0x00000010)
#define TYPHOON_OFFLOAD_BCAST_THROTTLE	__constant_cpu_to_le32(0x00000020)
#define TYPHOON_OFFLOAD_DHCP_PREVENT	__constant_cpu_to_le32(0x00000040)
#define TYPHOON_OFFLOAD_VLAN		__constant_cpu_to_le32(0x00000080)
#define TYPHOON_OFFLOAD_FILTERING	__constant_cpu_to_le32(0x00000100)
#define TYPHOON_OFFLOAD_TCP_SEGMENT	__constant_cpu_to_le32(0x00000200)

#define TYPHOON_WAKE_MAGIC_PKT		__constant_cpu_to_le16(0x01)
#define TYPHOON_WAKE_LINK_EVENT		__constant_cpu_to_le16(0x02)
#define TYPHOON_WAKE_ICMP_ECHO		__constant_cpu_to_le16(0x04)
#define TYPHOON_WAKE_ARP		__constant_cpu_to_le16(0x08)

struct typhoon_file_header {
	u8  tag[8];
	__le32 version;
	__le32 numSections;
	__le32 startAddr;
	__le32 hmacDigest[5];
} __attribute__ ((packed));

struct typhoon_section_header {
	__le32 len;
	u16 checksum;
	u16 reserved;
	__le32 startAddr;
} __attribute__ ((packed));

#define TYPHOON_REG_SOFT_RESET			0x00
#define TYPHOON_REG_INTR_STATUS			0x04
#define TYPHOON_REG_INTR_ENABLE			0x08
#define TYPHOON_REG_INTR_MASK			0x0c
#define TYPHOON_REG_SELF_INTERRUPT		0x10
#define TYPHOON_REG_HOST2ARM7			0x14
#define TYPHOON_REG_HOST2ARM6			0x18
#define TYPHOON_REG_HOST2ARM5			0x1c
#define TYPHOON_REG_HOST2ARM4			0x20
#define TYPHOON_REG_HOST2ARM3			0x24
#define TYPHOON_REG_HOST2ARM2			0x28
#define TYPHOON_REG_HOST2ARM1			0x2c
#define TYPHOON_REG_HOST2ARM0			0x30
#define TYPHOON_REG_ARM2HOST3			0x34
#define TYPHOON_REG_ARM2HOST2			0x38
#define TYPHOON_REG_ARM2HOST1			0x3c
#define TYPHOON_REG_ARM2HOST0			0x40

#define TYPHOON_REG_BOOT_DATA_LO		TYPHOON_REG_HOST2ARM5
#define TYPHOON_REG_BOOT_DATA_HI		TYPHOON_REG_HOST2ARM4
#define TYPHOON_REG_BOOT_DEST_ADDR		TYPHOON_REG_HOST2ARM3
#define TYPHOON_REG_BOOT_CHECKSUM		TYPHOON_REG_HOST2ARM2
#define TYPHOON_REG_BOOT_LENGTH			TYPHOON_REG_HOST2ARM1

#define TYPHOON_REG_DOWNLOAD_BOOT_ADDR		TYPHOON_REG_HOST2ARM1
#define TYPHOON_REG_DOWNLOAD_HMAC_0		TYPHOON_REG_HOST2ARM2
#define TYPHOON_REG_DOWNLOAD_HMAC_1		TYPHOON_REG_HOST2ARM3
#define TYPHOON_REG_DOWNLOAD_HMAC_2		TYPHOON_REG_HOST2ARM4
#define TYPHOON_REG_DOWNLOAD_HMAC_3		TYPHOON_REG_HOST2ARM5
#define TYPHOON_REG_DOWNLOAD_HMAC_4		TYPHOON_REG_HOST2ARM6

#define TYPHOON_REG_BOOT_RECORD_ADDR_HI		TYPHOON_REG_HOST2ARM2
#define TYPHOON_REG_BOOT_RECORD_ADDR_LO		TYPHOON_REG_HOST2ARM1

#define TYPHOON_REG_TX_LO_READY			TYPHOON_REG_HOST2ARM3
#define TYPHOON_REG_CMD_READY			TYPHOON_REG_HOST2ARM2
#define TYPHOON_REG_TX_HI_READY			TYPHOON_REG_HOST2ARM1

#define TYPHOON_REG_COMMAND			TYPHOON_REG_HOST2ARM0
#define TYPHOON_REG_HEARTBEAT			TYPHOON_REG_ARM2HOST3
#define TYPHOON_REG_STATUS			TYPHOON_REG_ARM2HOST0

#define TYPHOON_RESET_ALL	0x7f
#define TYPHOON_RESET_NONE	0x00

#define TYPHOON_INTR_HOST_INT		0x00000001
#define TYPHOON_INTR_ARM2HOST0		0x00000002
#define TYPHOON_INTR_ARM2HOST1		0x00000004
#define TYPHOON_INTR_ARM2HOST2		0x00000008
#define TYPHOON_INTR_ARM2HOST3		0x00000010
#define TYPHOON_INTR_DMA0		0x00000020
#define TYPHOON_INTR_DMA1		0x00000040
#define TYPHOON_INTR_DMA2		0x00000080
#define TYPHOON_INTR_DMA3		0x00000100
#define TYPHOON_INTR_MASTER_ABORT	0x00000200
#define TYPHOON_INTR_TARGET_ABORT	0x00000400
#define TYPHOON_INTR_SELF		0x00000800
#define TYPHOON_INTR_RESERVED		0xfffff000

#define TYPHOON_INTR_BOOTCMD		TYPHOON_INTR_ARM2HOST0

#define TYPHOON_INTR_ENABLE_ALL		0xffffffef
#define TYPHOON_INTR_ALL		0xffffffff
#define TYPHOON_INTR_NONE		0x00000000

#define TYPHOON_BOOTCMD_BOOT			0x00
#define TYPHOON_BOOTCMD_WAKEUP			0xfa
#define TYPHOON_BOOTCMD_DNLD_COMPLETE		0xfb
#define TYPHOON_BOOTCMD_SEG_AVAILABLE		0xfc
#define TYPHOON_BOOTCMD_RUNTIME_IMAGE		0xfd
#define TYPHOON_BOOTCMD_REG_BOOT_RECORD		0xff

#define TYPHOON_STATUS_WAITING_FOR_BOOT		0x07
#define TYPHOON_STATUS_SECOND_INIT		0x08
#define TYPHOON_STATUS_RUNNING			0x09
#define TYPHOON_STATUS_WAITING_FOR_HOST		0x0d
#define TYPHOON_STATUS_WAITING_FOR_SEGMENT	0x10
#define TYPHOON_STATUS_SLEEPING			0x11
#define TYPHOON_STATUS_HALTED			0x14
