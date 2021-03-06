
/* OLPC machine specific definitions */

#ifndef _ASM_X86_OLPC_H
#define _ASM_X86_OLPC_H

#include <asm/geode.h>

struct olpc_platform_t {
	int flags;
	uint32_t boardrev;
	int ecver;
};

#define OLPC_F_PRESENT		0x01
#define OLPC_F_DCON		0x02
#define OLPC_F_VSA		0x04

#ifdef CONFIG_OLPC

extern struct olpc_platform_t olpc_platform_info;


static inline uint32_t olpc_board(uint8_t id)
{
	return (id << 4) | 0x8;
}

static inline uint32_t olpc_board_pre(uint8_t id)
{
	return id << 4;
}

static inline int machine_is_olpc(void)
{
	return (olpc_platform_info.flags & OLPC_F_PRESENT) ? 1 : 0;
}

static inline int olpc_has_dcon(void)
{
	return (olpc_platform_info.flags & OLPC_F_DCON) ? 1 : 0;
}

static inline int olpc_has_vsa(void)
{
	return (olpc_platform_info.flags & OLPC_F_VSA) ? 1 : 0;
}

static inline int olpc_board_at_least(uint32_t rev)
{
	return olpc_platform_info.boardrev >= rev;
}

#else

static inline int machine_is_olpc(void)
{
	return 0;
}

static inline int olpc_has_dcon(void)
{
	return 0;
}

static inline int olpc_has_vsa(void)
{
	return 0;
}

#endif

/* EC related functions */

extern int olpc_ec_cmd(unsigned char cmd, unsigned char *inbuf, size_t inlen,
		unsigned char *outbuf, size_t outlen);

extern int olpc_ec_mask_set(uint8_t bits);
extern int olpc_ec_mask_unset(uint8_t bits);

/* EC commands */

#define EC_FIRMWARE_REV		0x08

/* SCI source values */

#define EC_SCI_SRC_EMPTY	0x00
#define EC_SCI_SRC_GAME		0x01
#define EC_SCI_SRC_BATTERY	0x02
#define EC_SCI_SRC_BATSOC	0x04
#define EC_SCI_SRC_BATERR	0x08
#define EC_SCI_SRC_EBOOK	0x10
#define EC_SCI_SRC_WLAN		0x20
#define EC_SCI_SRC_ACPWR	0x40
#define EC_SCI_SRC_ALL		0x7F

/* GPIO assignments */

#define OLPC_GPIO_MIC_AC	geode_gpio(1)
#define OLPC_GPIO_DCON_IRQ	geode_gpio(7)
#define OLPC_GPIO_THRM_ALRM	geode_gpio(10)
#define OLPC_GPIO_SMB_CLK	geode_gpio(14)
#define OLPC_GPIO_SMB_DATA	geode_gpio(15)
#define OLPC_GPIO_WORKAUX	geode_gpio(24)
#define OLPC_GPIO_LID		geode_gpio(26)
#define OLPC_GPIO_ECSCI		geode_gpio(27)

#endif /* _ASM_X86_OLPC_H */
