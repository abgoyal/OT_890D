

#ifndef _LINUX_TIPC_CONFIG_H_
#define _LINUX_TIPC_CONFIG_H_

#include <linux/types.h>
#include <linux/string.h>
#include <asm/byteorder.h>

 
 
#define  TIPC_CMD_NOOP   	    0x0000    /* tx none, rx none */
#define  TIPC_CMD_GET_NODES         0x0001    /* tx net_addr, rx node_info(s) */
#define  TIPC_CMD_GET_MEDIA_NAMES   0x0002    /* tx none, rx media_name(s) */
#define  TIPC_CMD_GET_BEARER_NAMES  0x0003    /* tx none, rx bearer_name(s) */
#define  TIPC_CMD_GET_LINKS         0x0004    /* tx net_addr, rx link_info(s) */
#define  TIPC_CMD_SHOW_NAME_TABLE   0x0005    /* tx name_tbl_query, rx ultra_string */
#define  TIPC_CMD_SHOW_PORTS        0x0006    /* tx none, rx ultra_string */
#define  TIPC_CMD_SHOW_LINK_STATS   0x000B    /* tx link_name, rx ultra_string */

#if 0
#define  TIPC_CMD_SHOW_PORT_STATS   0x0008    /* tx port_ref, rx ultra_string */
#define  TIPC_CMD_RESET_PORT_STATS  0x0009    /* tx port_ref, rx none */
#define  TIPC_CMD_GET_ROUTES        0x000A    /* tx ?, rx ? */
#define  TIPC_CMD_GET_LINK_PEER     0x000D    /* tx link_name, rx ? */
#endif


#define  TIPC_CMD_GET_REMOTE_MNG    0x4003    /* tx none, rx unsigned */
#define  TIPC_CMD_GET_MAX_PORTS     0x4004    /* tx none, rx unsigned */
#define  TIPC_CMD_GET_MAX_PUBL      0x4005    /* tx none, rx unsigned */
#define  TIPC_CMD_GET_MAX_SUBSCR    0x4006    /* tx none, rx unsigned */
#define  TIPC_CMD_GET_MAX_ZONES     0x4007    /* tx none, rx unsigned */
#define  TIPC_CMD_GET_MAX_CLUSTERS  0x4008    /* tx none, rx unsigned */
#define  TIPC_CMD_GET_MAX_NODES     0x4009    /* tx none, rx unsigned */
#define  TIPC_CMD_GET_MAX_SLAVES    0x400A    /* tx none, rx unsigned */
#define  TIPC_CMD_GET_NETID         0x400B    /* tx none, rx unsigned */

#define  TIPC_CMD_ENABLE_BEARER     0x4101    /* tx bearer_config, rx none */
#define  TIPC_CMD_DISABLE_BEARER    0x4102    /* tx bearer_name, rx none */
#define  TIPC_CMD_SET_LINK_TOL      0x4107    /* tx link_config, rx none */
#define  TIPC_CMD_SET_LINK_PRI      0x4108    /* tx link_config, rx none */
#define  TIPC_CMD_SET_LINK_WINDOW   0x4109    /* tx link_config, rx none */
#define  TIPC_CMD_SET_LOG_SIZE      0x410A    /* tx unsigned, rx none */
#define  TIPC_CMD_DUMP_LOG          0x410B    /* tx none, rx ultra_string */
#define  TIPC_CMD_RESET_LINK_STATS  0x410C    /* tx link_name, rx none */

#if 0
#define  TIPC_CMD_CREATE_LINK       0x4103    /* tx link_create, rx none */
#define  TIPC_CMD_REMOVE_LINK       0x4104    /* tx link_name, rx none */
#define  TIPC_CMD_BLOCK_LINK        0x4105    /* tx link_name, rx none */
#define  TIPC_CMD_UNBLOCK_LINK      0x4106    /* tx link_name, rx none */
#endif


#define  TIPC_CMD_SET_NODE_ADDR     0x8001    /* tx net_addr, rx none */
#if 0
#define  TIPC_CMD_SET_ZONE_MASTER   0x8002    /* tx none, rx none */
#endif
#define  TIPC_CMD_SET_REMOTE_MNG    0x8003    /* tx unsigned, rx none */
#define  TIPC_CMD_SET_MAX_PORTS     0x8004    /* tx unsigned, rx none */
#define  TIPC_CMD_SET_MAX_PUBL      0x8005    /* tx unsigned, rx none */
#define  TIPC_CMD_SET_MAX_SUBSCR    0x8006    /* tx unsigned, rx none */
#define  TIPC_CMD_SET_MAX_ZONES     0x8007    /* tx unsigned, rx none */
#define  TIPC_CMD_SET_MAX_CLUSTERS  0x8008    /* tx unsigned, rx none */
#define  TIPC_CMD_SET_MAX_NODES     0x8009    /* tx unsigned, rx none */
#define  TIPC_CMD_SET_MAX_SLAVES    0x800A    /* tx unsigned, rx none */
#define  TIPC_CMD_SET_NETID         0x800B    /* tx unsigned, rx none */


#define  TIPC_CMD_NOT_NET_ADMIN     0xC001    /* tx none, rx none */


#define TIPC_TLV_NONE		0	/* no TLV present */
#define TIPC_TLV_VOID		1	/* empty TLV (0 data bytes)*/
#define TIPC_TLV_UNSIGNED	2	/* 32-bit integer */
#define TIPC_TLV_STRING		3	/* char[128] (max) */
#define TIPC_TLV_LARGE_STRING	4	/* char[2048] (max) */
#define TIPC_TLV_ULTRA_STRING	5	/* char[32768] (max) */

#define TIPC_TLV_ERROR_STRING	16	/* char[128] containing "error code" */
#define TIPC_TLV_NET_ADDR   	17	/* 32-bit integer denoting <Z.C.N> */
#define TIPC_TLV_MEDIA_NAME	18	/* char[TIPC_MAX_MEDIA_NAME] */
#define TIPC_TLV_BEARER_NAME	19	/* char[TIPC_MAX_BEARER_NAME] */
#define TIPC_TLV_LINK_NAME	20	/* char[TIPC_MAX_LINK_NAME] */
#define TIPC_TLV_NODE_INFO	21	/* struct tipc_node_info */
#define TIPC_TLV_LINK_INFO	22	/* struct tipc_link_info */
#define TIPC_TLV_BEARER_CONFIG  23	/* struct tipc_bearer_config */
#define TIPC_TLV_LINK_CONFIG    24	/* struct tipc_link_config */
#define TIPC_TLV_NAME_TBL_QUERY	25	/* struct tipc_name_table_query */
#define TIPC_TLV_PORT_REF   	26	/* 32-bit port reference */


#define TIPC_MAX_MEDIA_NAME	16	/* format = media */
#define TIPC_MAX_IF_NAME	16	/* format = interface */
#define TIPC_MAX_BEARER_NAME	32	/* format = media:interface */
#define TIPC_MAX_LINK_NAME	60	/* format = Z.C.N:interface-Z.C.N:interface */


#define TIPC_MIN_LINK_PRI	0
#define TIPC_DEF_LINK_PRI	10
#define TIPC_MAX_LINK_PRI	31
#define TIPC_MEDIA_LINK_PRI	(TIPC_MAX_LINK_PRI + 1)


#define TIPC_MIN_LINK_TOL 50
#define TIPC_DEF_LINK_TOL 1500
#define TIPC_MAX_LINK_TOL 30000


#define TIPC_MIN_LINK_WIN 16
#define TIPC_DEF_LINK_WIN 50
#define TIPC_MAX_LINK_WIN 150


struct tipc_node_info {
	__be32 addr;			/* network address of node */
	__be32 up;			/* 0=down, 1= up */
};

struct tipc_link_info {
	__be32 dest;			/* network address of peer node */
	__be32 up;			/* 0=down, 1=up */
	char str[TIPC_MAX_LINK_NAME];	/* link name */
};

struct tipc_bearer_config {
	__be32 priority;		/* Range [1,31]. Override per link  */
	__be32 detect_scope;
	char name[TIPC_MAX_BEARER_NAME];
};

struct tipc_link_config {
	__be32 value;
	char name[TIPC_MAX_LINK_NAME];
};

#define TIPC_NTQ_ALLTYPES 0x80000000

struct tipc_name_table_query {
	__be32 depth;	/* 1:type, 2:+name info, 3:+port info, 4+:+debug info */
	__be32 type;	/* {t,l,u} info ignored if high bit of "depth" is set */
	__be32 lowbound; /* (i.e. displays all entries of name table) */
	__be32 upbound;
};


#define TIPC_CFG_TLV_ERROR      "\x80"  /* request contains incorrect TLV(s) */
#define TIPC_CFG_NOT_NET_ADMIN  "\x81"	/* must be network administrator */
#define TIPC_CFG_NOT_ZONE_MSTR	"\x82"	/* must be zone master */
#define TIPC_CFG_NO_REMOTE	"\x83"	/* remote management not enabled */
#define TIPC_CFG_NOT_SUPPORTED  "\x84"	/* request is not supported by TIPC */
#define TIPC_CFG_INVALID_VALUE  "\x85"  /* request has invalid argument value */

#if 0
/* prototypes TLV structures for proposed commands */
struct tipc_link_create {
	__u32   domain;
	struct tipc_media_addr peer_addr;
	char bearer_name[TIPC_MAX_BEARER_NAME];
};

struct tipc_route_info {
	__u32 dest;
	__u32 router;
};
#endif


struct tlv_desc {
	__be16 tlv_len;		/* TLV length (descriptor + value) */
	__be16 tlv_type;		/* TLV identifier */
};

#define TLV_ALIGNTO 4

#define TLV_ALIGN(datalen) (((datalen)+(TLV_ALIGNTO-1)) & ~(TLV_ALIGNTO-1))
#define TLV_LENGTH(datalen) (sizeof(struct tlv_desc) + (datalen))
#define TLV_SPACE(datalen) (TLV_ALIGN(TLV_LENGTH(datalen)))
#define TLV_DATA(tlv) ((void *)((char *)(tlv) + TLV_LENGTH(0)))

static inline int TLV_OK(const void *tlv, __u16 space)
{
	/*
	 * Would also like to check that "tlv" is a multiple of 4,
	 * but don't know how to do this in a portable way.
	 * - Tried doing (!(tlv & (TLV_ALIGNTO-1))), but GCC compiler
	 *   won't allow binary "&" with a pointer.
	 * - Tried casting "tlv" to integer type, but causes warning about size
	 *   mismatch when pointer is bigger than chosen type (int, long, ...).
	 */

	return (space >= TLV_SPACE(0)) &&
		(ntohs(((struct tlv_desc *)tlv)->tlv_len) <= space);
}

static inline int TLV_CHECK(const void *tlv, __u16 space, __u16 exp_type)
{
	return TLV_OK(tlv, space) && 
		(ntohs(((struct tlv_desc *)tlv)->tlv_type) == exp_type);
}

static inline int TLV_SET(void *tlv, __u16 type, void *data, __u16 len)
{
	struct tlv_desc *tlv_ptr;
	int tlv_len;

	tlv_len = TLV_LENGTH(len);
	tlv_ptr = (struct tlv_desc *)tlv;
	tlv_ptr->tlv_type = htons(type);
	tlv_ptr->tlv_len  = htons(tlv_len);
	if (len && data)
		memcpy(TLV_DATA(tlv_ptr), data, tlv_len);
	return TLV_SPACE(len);
}


struct tlv_list_desc {
	struct tlv_desc *tlv_ptr;	/* ptr to current TLV */
	__u32 tlv_space;		/* # bytes from curr TLV to list end */
};

static inline void TLV_LIST_INIT(struct tlv_list_desc *list, 
				 void *data, __u32 space)
{
	list->tlv_ptr = (struct tlv_desc *)data;
	list->tlv_space = space;
}
	     
static inline int TLV_LIST_EMPTY(struct tlv_list_desc *list)
{ 
	return (list->tlv_space == 0);
}

static inline int TLV_LIST_CHECK(struct tlv_list_desc *list, __u16 exp_type)
{
	return TLV_CHECK(list->tlv_ptr, list->tlv_space, exp_type);
}

static inline void *TLV_LIST_DATA(struct tlv_list_desc *list)
{
	return TLV_DATA(list->tlv_ptr);
}

static inline void TLV_LIST_STEP(struct tlv_list_desc *list)
{
	__u16 tlv_space = TLV_ALIGN(ntohs(list->tlv_ptr->tlv_len));

        list->tlv_ptr = (struct tlv_desc *)((char *)list->tlv_ptr + tlv_space);
	list->tlv_space -= tlv_space;
}

#define TIPC_GENL_NAME		"TIPC"
#define TIPC_GENL_VERSION	0x1
#define TIPC_GENL_CMD		0x1

struct tipc_genlmsghdr {
	__u32 dest;		/* Destination address */
	__u16 cmd;		/* Command */
	__u16 reserved;		/* Unused */
};

#define TIPC_GENL_HDRLEN	NLMSG_ALIGN(sizeof(struct tipc_genlmsghdr))


struct tipc_cfg_msg_hdr
{
	__be32 tcm_len;		/* Message length (including header) */
	__be16 tcm_type;	/* Command type */
	__be16 tcm_flags;	/* Additional flags */
	char  tcm_reserved[8];	/* Unused */
};

#define TCM_F_REQUEST	0x1	/* Flag: Request message */
#define TCM_F_MORE	0x2	/* Flag: Message to be continued */

#define TCM_ALIGN(datalen)  (((datalen)+3) & ~3)
#define TCM_LENGTH(datalen) (sizeof(struct tipc_cfg_msg_hdr) + datalen)
#define TCM_SPACE(datalen)  (TCM_ALIGN(TCM_LENGTH(datalen)))
#define TCM_DATA(tcm_hdr)   ((void *)((char *)(tcm_hdr) + TCM_LENGTH(0)))

static inline int TCM_SET(void *msg, __u16 cmd, __u16 flags,
			  void *data, __u16 data_len)
{
	struct tipc_cfg_msg_hdr *tcm_hdr;
	int msg_len;

	msg_len = TCM_LENGTH(data_len);
	tcm_hdr = (struct tipc_cfg_msg_hdr *)msg;
	tcm_hdr->tcm_len   = htonl(msg_len);
	tcm_hdr->tcm_type  = htons(cmd);
	tcm_hdr->tcm_flags = htons(flags);
	if (data_len && data)
		memcpy(TCM_DATA(msg), data, data_len);
	return TCM_SPACE(data_len);
}

#endif
