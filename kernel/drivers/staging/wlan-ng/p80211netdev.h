

#ifndef _LINUX_P80211NETDEV_H
#define _LINUX_P80211NETDEV_H

#include <linux/interrupt.h>
#include <linux/wireless.h>

/*================================================================*/
/* Constants */

#define WLAN_DEVICE_CLOSED	0
#define WLAN_DEVICE_OPEN	1

#define WLAN_MACMODE_NONE	0
#define WLAN_MACMODE_IBSS_STA	1
#define WLAN_MACMODE_ESS_STA	2
#define WLAN_MACMODE_ESS_AP	3

/* MSD States */
#define WLAN_MSD_START			-1
#define WLAN_MSD_DRIVERLOADED		0
#define WLAN_MSD_HWPRESENT_PENDING	1
#define WLAN_MSD_HWFAIL			2
#define WLAN_MSD_HWPRESENT		3
#define WLAN_MSD_FWLOAD_PENDING		4
#define WLAN_MSD_FWLOAD			5
#define WLAN_MSD_RUNNING_PENDING	6
#define WLAN_MSD_RUNNING		7

#ifndef ETH_P_ECONET
#define ETH_P_ECONET   0x0018    /* needed for 2.2.x kernels */
#endif

#define ETH_P_80211_RAW        (ETH_P_ECONET + 1)

#ifndef ARPHRD_IEEE80211
#define ARPHRD_IEEE80211 801     /* kernel 2.4.6 */
#endif

#ifndef ARPHRD_IEEE80211_PRISM  /* kernel 2.4.18 */
#define ARPHRD_IEEE80211_PRISM 802
#endif

/*--- NSD Capabilities Flags ------------------------------*/
#define P80211_NSDCAP_HARDWAREWEP           0x01  /* hardware wep engine */
#define P80211_NSDCAP_TIEDWEP               0x02  /* can't decouple en/de */
#define P80211_NSDCAP_NOHOSTWEP             0x04  /* must use hardware wep */
#define P80211_NSDCAP_PBCC                  0x08  /* hardware supports PBCC */
#define P80211_NSDCAP_SHORT_PREAMBLE        0x10  /* hardware supports */
#define P80211_NSDCAP_AGILITY               0x20  /* hardware supports */
#define P80211_NSDCAP_AP_RETRANSMIT         0x40  /* nsd handles retransmits */
#define P80211_NSDCAP_HWFRAGMENT            0x80  /* nsd handles frag/defrag */
#define P80211_NSDCAP_AUTOJOIN              0x100  /* nsd does autojoin */
#define P80211_NSDCAP_NOSCAN                0x200  /* nsd can scan */

/*================================================================*/
/* Macros */

/*================================================================*/
/* Types */

/* Received frame statistics */
typedef struct p80211_frmrx_t
{
	u32	mgmt;
	u32	assocreq;
	u32	assocresp;
	u32	reassocreq;
	u32	reassocresp;
	u32	probereq;
	u32	proberesp;
	u32	beacon;
	u32	atim;
	u32	disassoc;
	u32	authen;
	u32	deauthen;
	u32	mgmt_unknown;
	u32	ctl;
	u32	pspoll;
	u32	rts;
	u32	cts;
	u32	ack;
	u32	cfend;
	u32	cfendcfack;
	u32	ctl_unknown;
	u32	data;
	u32	dataonly;
	u32	data_cfack;
	u32	data_cfpoll;
	u32	data__cfack_cfpoll;
	u32	null;
	u32	cfack;
	u32	cfpoll;
	u32	cfack_cfpoll;
	u32	data_unknown;
	u32  decrypt;
	u32  decrypt_err;
} p80211_frmrx_t;

/* called by /proc/net/wireless */
struct iw_statistics* p80211wext_get_wireless_stats(netdevice_t *dev);
/* wireless extensions' ioctls */
int p80211wext_support_ioctl(netdevice_t *dev, struct ifreq *ifr, int cmd);
extern struct iw_handler_def p80211wext_handler_def;
int p80211wext_event_associated(struct wlandevice *wlandev, int assoc);

/* WEP stuff */
#define NUM_WEPKEYS 4
#define MAX_KEYLEN 32

#define HOSTWEP_DEFAULTKEY_MASK (BIT1|BIT0)
#define HOSTWEP_DECRYPT  BIT4
#define HOSTWEP_ENCRYPT  BIT5
#define HOSTWEP_PRIVACYINVOKED BIT6
#define HOSTWEP_EXCLUDEUNENCRYPTED BIT7

extern int wlan_watchdog;
extern int wlan_wext_write;

/* WLAN device type */
typedef struct wlandevice
{
	struct wlandevice	*next;		/* link for list of devices */
	void			*priv;		/* private data for MSD */

	/* Subsystem State */
	char		name[WLAN_DEVNAMELEN_MAX]; /* Dev name, from register_wlandev()*/
	char		*nsdname;

	u32          state;          /* Device I/F state (open/closed) */
	u32		msdstate;	/* state of underlying driver */
	u32		hwremoved;	/* Has the hw been yanked out? */

	/* Hardware config */
	unsigned int		irq;
	unsigned int		iobase;
	unsigned int		membase;
	u32          nsdcaps;  /* NSD Capabilities flags */

	/* Config vars */
	unsigned int		ethconv;

	/* device methods (init by MSD, used by p80211 */
	int		(*open)(struct wlandevice *wlandev);
	int		(*close)(struct wlandevice *wlandev);
	void		(*reset)(struct wlandevice *wlandev );
	int		(*txframe)(struct wlandevice *wlandev, struct sk_buff *skb, p80211_hdr_t *p80211_hdr, p80211_metawep_t *p80211_wep);
	int		(*mlmerequest)(struct wlandevice *wlandev, p80211msg_t *msg);
	int             (*set_multicast_list)(struct wlandevice *wlandev,
					      netdevice_t *dev);
	void		(*tx_timeout)(struct wlandevice *wlandev);

	/* 802.11 State */
	u8		bssid[WLAN_BSSID_LEN];
	p80211pstr32_t	ssid;
	u32		macmode;
	int             linkstatus;

	/* WEP State */
	u8 wep_keys[NUM_WEPKEYS][MAX_KEYLEN];
	u8 wep_keylens[NUM_WEPKEYS];
	int   hostwep;

	/* Request/Confirm i/f state (used by p80211) */
	unsigned long		request_pending; /* flag, access atomically */

	/* netlink socket */
	/* queue for indications waiting for cmd completion */
	/* Linux netdevice and support */
	netdevice_t		*netdev;	/* ptr to linux netdevice */
	struct net_device_stats linux_stats;

	/* Rx bottom half */
	struct tasklet_struct	rx_bh;

	struct sk_buff_head	nsd_rxq;

	/* 802.11 device statistics */
	struct p80211_frmrx_t	rx;

	struct iw_statistics	wstats;

	/* jkriegl: iwspy fields */
        u8			spy_number;
        char			spy_address[IW_MAX_SPY][ETH_ALEN];
        struct iw_quality       spy_stat[IW_MAX_SPY];
} wlandevice_t;

/* WEP stuff */
int wep_change_key(wlandevice_t *wlandev, int keynum, u8* key, int keylen);
int wep_decrypt(wlandevice_t *wlandev, u8 *buf, u32 len, int key_override, u8 *iv, u8 *icv);
int wep_encrypt(wlandevice_t *wlandev, u8 *buf, u8 *dst, u32 len, int keynum, u8 *iv, u8 *icv);

void	p80211netdev_startup(void);
void	p80211netdev_shutdown(void);
int	wlan_setup(wlandevice_t *wlandev);
int	wlan_unsetup(wlandevice_t *wlandev);
int	register_wlandev(wlandevice_t *wlandev);
int	unregister_wlandev(wlandevice_t *wlandev);
void	p80211netdev_rx(wlandevice_t *wlandev, struct sk_buff *skb);
void	p80211netdev_hwremoved(wlandevice_t *wlandev);

#endif