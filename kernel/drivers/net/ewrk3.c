

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/crc32.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/ctype.h>
#include <linux/bitops.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/uaccess.h>

#include "ewrk3.h"

#define DRV_NAME	"ewrk3"
#define DRV_VERSION	"0.48"

static char version[] __initdata =
DRV_NAME ":v" DRV_VERSION " 2002/10/18 davies@maniac.ultranet.com\n";

#ifdef EWRK3_DEBUG
static int ewrk3_debug = EWRK3_DEBUG;
#else
static int ewrk3_debug = 1;
#endif

#define EWRK3_NDA 0xffe0	/* No Device Address */

#define PROBE_LENGTH    32
#define ETH_PROM_SIG    0xAA5500FFUL

#ifndef EWRK3_SIGNATURE
#define EWRK3_SIGNATURE {"DE203","DE204","DE205",""}
#define EWRK3_STRLEN 8
#endif

#ifndef EWRK3_RAM_BASE_ADDRESSES
#define EWRK3_RAM_BASE_ADDRESSES {0xc0000,0xd0000,0x00000}
#endif

#define EWRK3_IO_BASE 0x100	/* Start address for probe search */
#define EWRK3_IOP_INC 0x20	/* I/O address increment */
#define EWRK3_TOTAL_SIZE 0x20	/* required I/O address length */

#ifndef MAX_NUM_EWRK3S
#define MAX_NUM_EWRK3S 21
#endif

#ifndef EWRK3_EISA_IO_PORTS
#define EWRK3_EISA_IO_PORTS 0x0c00	/* I/O port base address, slot 0 */
#endif

#ifndef MAX_EISA_SLOTS
#define MAX_EISA_SLOTS 16
#define EISA_SLOT_INC 0x1000
#endif

#define QUEUE_PKT_TIMEOUT (1*HZ)	/* Jiffies */

#define IO_ONLY         0x00
#define SHMEM_2K        0x800
#define SHMEM_32K       0x8000
#define SHMEM_64K       0x10000

#define ENABLE_IRQs { \
  icr |= lp->irq_mask;\
  outb(icr, EWRK3_ICR);                     /* Enable the IRQs */\
}

#define DISABLE_IRQs { \
  icr = inb(EWRK3_ICR);\
  icr &= ~lp->irq_mask;\
  outb(icr, EWRK3_ICR);                     /* Disable the IRQs */\
}

#define START_EWRK3 { \
  csr = inb(EWRK3_CSR);\
  csr &= ~(CSR_TXD|CSR_RXD);\
  outb(csr, EWRK3_CSR);                     /* Enable the TX and/or RX */\
}

#define STOP_EWRK3 { \
  csr = (CSR_TXD|CSR_RXD);\
  outb(csr, EWRK3_CSR);                     /* Disable the TX and/or RX */\
}

#define EWRK3_PKT_STAT_SZ 16
#define EWRK3_PKT_BIN_SZ  128	/* Should be >=100 unless you
				   increase EWRK3_PKT_STAT_SZ */

struct ewrk3_stats {
	u32 bins[EWRK3_PKT_STAT_SZ];
	u32 unicast;
	u32 multicast;
	u32 broadcast;
	u32 excessive_collisions;
	u32 tx_underruns;
	u32 excessive_underruns;
};

struct ewrk3_private {
	char adapter_name[80];	/* Name exported to /proc/ioports */
	u_long shmem_base;	/* Shared memory start address */
	void __iomem *shmem;
	u_long shmem_length;	/* Shared memory window length */
	struct ewrk3_stats pktStats; /* Private stats counters */
	u_char irq_mask;	/* Adapter IRQ mask bits */
	u_char mPage;		/* Maximum 2kB Page number */
	u_char lemac;		/* Chip rev. level */
	u_char hard_strapped;	/* Don't allow a full open */
	u_char txc;		/* Transmit cut through */
	void __iomem *mctbl;	/* Pointer to the multicast table */
	u_char led_mask;	/* Used to reserve LED access for ethtool */
	spinlock_t hw_lock;
};

#define FORCE_2K_MODE { \
  shmem_length = SHMEM_2K;\
  outb(((mem_start - 0x80000) >> 11), EWRK3_MBR);\
}

static int ewrk3_open(struct net_device *dev);
static int ewrk3_queue_pkt(struct sk_buff *skb, struct net_device *dev);
static irqreturn_t ewrk3_interrupt(int irq, void *dev_id);
static int ewrk3_close(struct net_device *dev);
static void set_multicast_list(struct net_device *dev);
static int ewrk3_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static const struct ethtool_ops ethtool_ops_203;
static const struct ethtool_ops ethtool_ops;

static int ewrk3_hw_init(struct net_device *dev, u_long iobase);
static void ewrk3_init(struct net_device *dev);
static int ewrk3_rx(struct net_device *dev);
static int ewrk3_tx(struct net_device *dev);
static void ewrk3_timeout(struct net_device *dev);

static void EthwrkSignature(char *name, char *eeprom_image);
static int DevicePresent(u_long iobase);
static void SetMulticastFilter(struct net_device *dev);
static int EISA_signature(char *name, s32 eisa_id);

static int Read_EEPROM(u_long iobase, u_char eaddr);
static int Write_EEPROM(short data, u_long iobase, u_char eaddr);
static u_char get_hw_addr(struct net_device *dev, u_char * eeprom_image, char chipType);

static int ewrk3_probe1(struct net_device *dev, u_long iobase, int irq);
static int isa_probe(struct net_device *dev, u_long iobase);
static int eisa_probe(struct net_device *dev, u_long iobase);

static u_char irq[MAX_NUM_EWRK3S+1] = {5, 0, 10, 3, 11, 9, 15, 12};

static char name[EWRK3_STRLEN + 1];
static int num_ewrks3s;

#define INIT_EWRK3 {\
    outb(EEPROM_INIT, EWRK3_IOPR);\
    mdelay(1);\
}

#ifndef MODULE
struct net_device * __init ewrk3_probe(int unit)
{
	struct net_device *dev = alloc_etherdev(sizeof(struct ewrk3_private));
	int err;

	if (!dev)
		return ERR_PTR(-ENOMEM);

	if (unit >= 0) {
		sprintf(dev->name, "eth%d", unit);
		netdev_boot_setup_check(dev);
	}

	err = ewrk3_probe1(dev, dev->base_addr, dev->irq);
	if (err)
		goto out;
	return dev;
out:
	free_netdev(dev);
	return ERR_PTR(err);

}
#endif

static int __init ewrk3_probe1(struct net_device *dev, u_long iobase, int irq)
{
	int err;

	dev->base_addr = iobase;
	dev->irq = irq;

	/* Address PROM pattern */
	err = isa_probe(dev, iobase);
	if (err != 0)
		err = eisa_probe(dev, iobase);

	if (err)
		return err;

	err = register_netdev(dev);
	if (err)
		release_region(dev->base_addr, EWRK3_TOTAL_SIZE);

	return err;
}

static int __init
ewrk3_hw_init(struct net_device *dev, u_long iobase)
{
	struct ewrk3_private *lp;
	int i, status = 0;
	u_long mem_start, shmem_length;
	u_char cr, cmr, icr, nicsr, lemac, hard_strapped = 0;
	u_char eeprom_image[EEPROM_MAX], chksum, eisa_cr = 0;

	/*
	** Stop the EWRK3. Enable the DBR ROM. Disable interrupts and remote boot.
	** This also disables the EISA_ENABLE bit in the EISA Control Register.
	 */
	if (iobase > 0x400)
		eisa_cr = inb(EISA_CR);
	INIT_EWRK3;

	nicsr = inb(EWRK3_CSR);

	icr = inb(EWRK3_ICR);
	icr &= 0x70;
	outb(icr, EWRK3_ICR);	/* Disable all the IRQs */

	if (nicsr != (CSR_TXD | CSR_RXD))
		return -ENXIO;

	/* Check that the EEPROM is alive and well and not living on Pluto... */
	for (chksum = 0, i = 0; i < EEPROM_MAX; i += 2) {
		union {
			short val;
			char c[2];
		} tmp;

		tmp.val = (short) Read_EEPROM(iobase, (i >> 1));
		eeprom_image[i] = tmp.c[0];
		eeprom_image[i + 1] = tmp.c[1];
		chksum += eeprom_image[i] + eeprom_image[i + 1];
	}

	if (chksum != 0) {	/* Bad EEPROM Data! */
		printk("%s: Device has a bad on-board EEPROM.\n", dev->name);
		return -ENXIO;
	}

	EthwrkSignature(name, eeprom_image);
	if (*name == '\0')
		return -ENXIO;

	dev->base_addr = iobase;

	if (iobase > 0x400) {
		outb(eisa_cr, EISA_CR);		/* Rewrite the EISA CR */
	}
	lemac = eeprom_image[EEPROM_CHIPVER];
	cmr = inb(EWRK3_CMR);

	if (((lemac == LeMAC) && ((cmr & CMR_NO_EEPROM) != CMR_NO_EEPROM)) ||
	    ((lemac == LeMAC2) && !(cmr & CMR_HS))) {
		printk("%s: %s at %#4lx", dev->name, name, iobase);
		hard_strapped = 1;
	} else if ((iobase & 0x0fff) == EWRK3_EISA_IO_PORTS) {
		/* EISA slot address */
		printk("%s: %s at %#4lx (EISA slot %ld)",
		       dev->name, name, iobase, ((iobase >> 12) & 0x0f));
	} else {	/* ISA port address */
		printk("%s: %s at %#4lx", dev->name, name, iobase);
	}

	printk(", h/w address ");
	if (lemac != LeMAC2)
		DevicePresent(iobase);	/* need after EWRK3_INIT */
	status = get_hw_addr(dev, eeprom_image, lemac);
	printk("%pM\n", dev->dev_addr);

	if (status) {
		printk("      which has an EEPROM CRC error.\n");
		return -ENXIO;
	}

	if (lemac == LeMAC2) {	/* Special LeMAC2 CMR things */
		cmr &= ~(CMR_RA | CMR_WB | CMR_LINK | CMR_POLARITY | CMR_0WS);
		if (eeprom_image[EEPROM_MISC0] & READ_AHEAD)
			cmr |= CMR_RA;
		if (eeprom_image[EEPROM_MISC0] & WRITE_BEHIND)
			cmr |= CMR_WB;
		if (eeprom_image[EEPROM_NETMAN0] & NETMAN_POL)
			cmr |= CMR_POLARITY;
		if (eeprom_image[EEPROM_NETMAN0] & NETMAN_LINK)
			cmr |= CMR_LINK;
		if (eeprom_image[EEPROM_MISC0] & _0WS_ENA)
			cmr |= CMR_0WS;
	}
	if (eeprom_image[EEPROM_SETUP] & SETUP_DRAM)
		cmr |= CMR_DRAM;
	outb(cmr, EWRK3_CMR);

	cr = inb(EWRK3_CR);	/* Set up the Control Register */
	cr |= eeprom_image[EEPROM_SETUP] & SETUP_APD;
	if (cr & SETUP_APD)
		cr |= eeprom_image[EEPROM_SETUP] & SETUP_PS;
	cr |= eeprom_image[EEPROM_MISC0] & FAST_BUS;
	cr |= eeprom_image[EEPROM_MISC0] & ENA_16;
	outb(cr, EWRK3_CR);

	/*
	** Determine the base address and window length for the EWRK3
	** RAM from the memory base register.
	*/
	mem_start = inb(EWRK3_MBR);
	shmem_length = 0;
	if (mem_start != 0) {
		if ((mem_start >= 0x0a) && (mem_start <= 0x0f)) {
			mem_start *= SHMEM_64K;
			shmem_length = SHMEM_64K;
		} else if ((mem_start >= 0x14) && (mem_start <= 0x1f)) {
			mem_start *= SHMEM_32K;
			shmem_length = SHMEM_32K;
		} else if ((mem_start >= 0x40) && (mem_start <= 0xff)) {
			mem_start = mem_start * SHMEM_2K + 0x80000;
			shmem_length = SHMEM_2K;
		} else {
			return -ENXIO;
		}
	}
	/*
	** See the top of this source code for comments about
	** uncommenting this line.
	*/
/*          FORCE_2K_MODE; */

	if (hard_strapped) {
		printk("      is hard strapped.\n");
	} else if (mem_start) {
		printk("      has a %dk RAM window", (int) (shmem_length >> 10));
		printk(" at 0x%.5lx", mem_start);
	} else {
		printk("      is in I/O only mode");
	}

	lp = netdev_priv(dev);
	lp->shmem_base = mem_start;
	lp->shmem = ioremap(mem_start, shmem_length);
	if (!lp->shmem)
		return -ENOMEM;
	lp->shmem_length = shmem_length;
	lp->lemac = lemac;
	lp->hard_strapped = hard_strapped;
	lp->led_mask = CR_LED;
	spin_lock_init(&lp->hw_lock);

	lp->mPage = 64;
	if (cmr & CMR_DRAM)
		lp->mPage <<= 1;	/* 2 DRAMS on module */

	sprintf(lp->adapter_name, "%s (%s)", name, dev->name);

	lp->irq_mask = ICR_TNEM | ICR_TXDM | ICR_RNEM | ICR_RXDM;

	if (!hard_strapped) {
		/*
		** Enable EWRK3 board interrupts for autoprobing
		*/
		icr |= ICR_IE;	/* Enable interrupts */
		outb(icr, EWRK3_ICR);

		/* The DMA channel may be passed in on this parameter. */
		dev->dma = 0;

		/* To auto-IRQ we enable the initialization-done and DMA err,
		   interrupts. For now we will always get a DMA error. */
		if (dev->irq < 2) {
#ifndef MODULE
			u_char irqnum;
			unsigned long irq_mask;


			irq_mask = probe_irq_on();

			/*
			** Trigger a TNE interrupt.
			*/
			icr |= ICR_TNEM;
			outb(1, EWRK3_TDQ);	/* Write to the TX done queue */
			outb(icr, EWRK3_ICR);	/* Unmask the TXD interrupt */

			irqnum = irq[((icr & IRQ_SEL) >> 4)];

			mdelay(20);
			dev->irq = probe_irq_off(irq_mask);
			if ((dev->irq) && (irqnum == dev->irq)) {
				printk(" and uses IRQ%d.\n", dev->irq);
			} else {
				if (!dev->irq) {
					printk(" and failed to detect IRQ line.\n");
				} else if ((irqnum == 1) && (lemac == LeMAC2)) {
					printk(" and an illegal IRQ line detected.\n");
				} else {
					printk(", but incorrect IRQ line detected.\n");
				}
				iounmap(lp->shmem);
				return -ENXIO;
			}

			DISABLE_IRQs;	/* Mask all interrupts */

#endif				/* MODULE */
		} else {
			printk(" and requires IRQ%d.\n", dev->irq);
		}
	}

	if (ewrk3_debug > 1) {
		printk(version);
	}
	/* The EWRK3-specific entries in the device structure. */
	dev->open = ewrk3_open;
	dev->hard_start_xmit = ewrk3_queue_pkt;
	dev->stop = ewrk3_close;
	dev->set_multicast_list = set_multicast_list;
	dev->do_ioctl = ewrk3_ioctl;
	if (lp->adapter_name[4] == '3')
		SET_ETHTOOL_OPS(dev, &ethtool_ops_203);
	else
		SET_ETHTOOL_OPS(dev, &ethtool_ops);
	dev->tx_timeout = ewrk3_timeout;
	dev->watchdog_timeo = QUEUE_PKT_TIMEOUT;

	dev->mem_start = 0;

	return 0;
}


static int ewrk3_open(struct net_device *dev)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	u_long iobase = dev->base_addr;
	int status = 0;
	u_char icr, csr;

	/*
	   ** Stop the TX and RX...
	 */
	STOP_EWRK3;

	if (!lp->hard_strapped) {
		if (request_irq(dev->irq, (void *) ewrk3_interrupt, 0, "ewrk3", dev)) {
			printk("ewrk3_open(): Requested IRQ%d is busy\n", dev->irq);
			status = -EAGAIN;
		} else {

			/*
			   ** Re-initialize the EWRK3...
			 */
			ewrk3_init(dev);

			if (ewrk3_debug > 1) {
				printk("%s: ewrk3 open with irq %d\n", dev->name, dev->irq);
				printk("  physical address: %pM\n", dev->dev_addr);
				if (lp->shmem_length == 0) {
					printk("  no shared memory, I/O only mode\n");
				} else {
					printk("  start of shared memory: 0x%08lx\n", lp->shmem_base);
					printk("  window length: 0x%04lx\n", lp->shmem_length);
				}
				printk("  # of DRAMS: %d\n", ((inb(EWRK3_CMR) & 0x02) ? 2 : 1));
				printk("  csr:  0x%02x\n", inb(EWRK3_CSR));
				printk("  cr:   0x%02x\n", inb(EWRK3_CR));
				printk("  icr:  0x%02x\n", inb(EWRK3_ICR));
				printk("  cmr:  0x%02x\n", inb(EWRK3_CMR));
				printk("  fmqc: 0x%02x\n", inb(EWRK3_FMQC));
			}
			netif_start_queue(dev);
			/*
			   ** Unmask EWRK3 board interrupts
			 */
			icr = inb(EWRK3_ICR);
			ENABLE_IRQs;

		}
	} else {
		printk(KERN_ERR "%s: ewrk3 available for hard strapped set up only.\n", dev->name);
		printk(KERN_ERR "      Run the 'ewrk3setup' utility or remove the hard straps.\n");
		return -EINVAL;
	}

	return status;
}

static void ewrk3_init(struct net_device *dev)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	u_char csr, page;
	u_long iobase = dev->base_addr;
	int i;

	/*
	   ** Enable any multicasts
	 */
	set_multicast_list(dev);

	/*
	** Set hardware MAC address. Address is initialized from the EEPROM
	** during startup but may have since been changed by the user.
	*/
	for (i=0; i<ETH_ALEN; i++)
		outb(dev->dev_addr[i], EWRK3_PAR0 + i);

	/*
	   ** Clean out any remaining entries in all the queues here
	 */
	while (inb(EWRK3_TQ));
	while (inb(EWRK3_TDQ));
	while (inb(EWRK3_RQ));
	while (inb(EWRK3_FMQ));

	/*
	   ** Write a clean free memory queue
	 */
	for (page = 1; page < lp->mPage; page++) {	/* Write the free page numbers */
		outb(page, EWRK3_FMQ);	/* to the Free Memory Queue */
	}

	START_EWRK3;		/* Enable the TX and/or RX */
}


static void ewrk3_timeout(struct net_device *dev)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	u_char icr, csr;
	u_long iobase = dev->base_addr;

	if (!lp->hard_strapped)
	{
		printk(KERN_WARNING"%s: transmit timed/locked out, status %04x, resetting.\n",
		       dev->name, inb(EWRK3_CSR));

		/*
		   ** Mask all board interrupts
		 */
		DISABLE_IRQs;

		/*
		   ** Stop the TX and RX...
		 */
		STOP_EWRK3;

		ewrk3_init(dev);

		/*
		   ** Unmask EWRK3 board interrupts
		 */
		ENABLE_IRQs;

		dev->trans_start = jiffies;
		netif_wake_queue(dev);
	}
}

static int ewrk3_queue_pkt (struct sk_buff *skb, struct net_device *dev)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	u_long iobase = dev->base_addr;
	void __iomem *buf = NULL;
	u_char icr;
	u_char page;

	spin_lock_irq (&lp->hw_lock);
	DISABLE_IRQs;

	/* if no resources available, exit, request packet be queued */
	if (inb (EWRK3_FMQC) == 0) {
		printk (KERN_WARNING "%s: ewrk3_queue_pkt(): No free resources...\n",
			dev->name);
		printk (KERN_WARNING "%s: ewrk3_queue_pkt(): CSR: %02x ICR: %02x FMQC: %02x\n",
			dev->name, inb (EWRK3_CSR), inb (EWRK3_ICR),
			inb (EWRK3_FMQC));
		goto err_out;
	}

	/*
	 ** Get a free page from the FMQ
	 */
	if ((page = inb (EWRK3_FMQ)) >= lp->mPage) {
		printk ("ewrk3_queue_pkt(): Invalid free memory page (%d).\n",
		     (u_char) page);
		goto err_out;
	}


	/*
	 ** Set up shared memory window and pointer into the window
	 */
	if (lp->shmem_length == IO_ONLY) {
		outb (page, EWRK3_IOPR);
	} else if (lp->shmem_length == SHMEM_2K) {
		buf = lp->shmem;
		outb (page, EWRK3_MPR);
	} else if (lp->shmem_length == SHMEM_32K) {
		buf = (((short) page << 11) & 0x7800) + lp->shmem;
		outb ((page >> 4), EWRK3_MPR);
	} else if (lp->shmem_length == SHMEM_64K) {
		buf = (((short) page << 11) & 0xf800) + lp->shmem;
		outb ((page >> 5), EWRK3_MPR);
	} else {
		printk (KERN_ERR "%s: Oops - your private data area is hosed!\n",
			dev->name);
		BUG ();
	}

	/*
	 ** Set up the buffer control structures and copy the data from
	 ** the socket buffer to the shared memory .
	 */
	if (lp->shmem_length == IO_ONLY) {
		int i;
		u_char *p = skb->data;
		outb ((char) (TCR_QMODE | TCR_PAD | TCR_IFC), EWRK3_DATA);
		outb ((char) (skb->len & 0xff), EWRK3_DATA);
		outb ((char) ((skb->len >> 8) & 0xff), EWRK3_DATA);
		outb ((char) 0x04, EWRK3_DATA);
		for (i = 0; i < skb->len; i++) {
			outb (*p++, EWRK3_DATA);
		}
		outb (page, EWRK3_TQ);	/* Start sending pkt */
	} else {
		writeb ((char) (TCR_QMODE | TCR_PAD | TCR_IFC), buf);	/* ctrl byte */
		buf += 1;
		writeb ((char) (skb->len & 0xff), buf);	/* length (16 bit xfer) */
		buf += 1;
		if (lp->txc) {
			writeb(((skb->len >> 8) & 0xff) | XCT, buf);
			buf += 1;
			writeb (0x04, buf);	/* index byte */
			buf += 1;
			writeb (0x00, (buf + skb->len));	/* Write the XCT flag */
			memcpy_toio (buf, skb->data, PRELOAD);	/* Write PRELOAD bytes */
			outb (page, EWRK3_TQ);	/* Start sending pkt */
			memcpy_toio (buf + PRELOAD,
					 skb->data + PRELOAD,
					 skb->len - PRELOAD);
			writeb (0xff, (buf + skb->len));	/* Write the XCT flag */
		} else {
			writeb ((skb->len >> 8) & 0xff, buf);
			buf += 1;
			writeb (0x04, buf);	/* index byte */
			buf += 1;
			memcpy_toio (buf, skb->data, skb->len);	/* Write data bytes */
			outb (page, EWRK3_TQ);	/* Start sending pkt */
		}
	}

	ENABLE_IRQs;
	spin_unlock_irq (&lp->hw_lock);

	dev->stats.tx_bytes += skb->len;
	dev->trans_start = jiffies;
	dev_kfree_skb (skb);

	/* Check for free resources: stop Tx queue if there are none */
	if (inb (EWRK3_FMQC) == 0)
		netif_stop_queue (dev);

	return 0;

err_out:
	ENABLE_IRQs;
	spin_unlock_irq (&lp->hw_lock);
	return 1;
}

static irqreturn_t ewrk3_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct ewrk3_private *lp;
	u_long iobase;
	u_char icr, cr, csr;

	lp = netdev_priv(dev);
	iobase = dev->base_addr;

	/* get the interrupt information */
	csr = inb(EWRK3_CSR);

	/*
	 ** Mask the EWRK3 board interrupts and turn on the LED
	 */
	spin_lock(&lp->hw_lock);
	DISABLE_IRQs;

	cr = inb(EWRK3_CR);
	cr |= lp->led_mask;
	outb(cr, EWRK3_CR);

	if (csr & CSR_RNE)	/* Rx interrupt (packet[s] arrived) */
		ewrk3_rx(dev);

	if (csr & CSR_TNE)	/* Tx interrupt (packet sent) */
		ewrk3_tx(dev);

	/*
	 ** Now deal with the TX/RX disable flags. These are set when there
	 ** are no more resources. If resources free up then enable these
	 ** interrupts, otherwise mask them - failure to do this will result
	 ** in the system hanging in an interrupt loop.
	 */
	if (inb(EWRK3_FMQC)) {	/* any resources available? */
		lp->irq_mask |= ICR_TXDM | ICR_RXDM;	/* enable the interrupt source */
		csr &= ~(CSR_TXD | CSR_RXD);	/* ensure restart of a stalled TX or RX */
		outb(csr, EWRK3_CSR);
		netif_wake_queue(dev);
	} else {
		lp->irq_mask &= ~(ICR_TXDM | ICR_RXDM);		/* disable the interrupt source */
	}

	/* Unmask the EWRK3 board interrupts and turn off the LED */
	cr &= ~(lp->led_mask);
	outb(cr, EWRK3_CR);
	ENABLE_IRQs;
	spin_unlock(&lp->hw_lock);
	return IRQ_HANDLED;
}

/* Called with lp->hw_lock held */
static int ewrk3_rx(struct net_device *dev)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	u_long iobase = dev->base_addr;
	int i, status = 0;
	u_char page;
	void __iomem *buf = NULL;

	while (inb(EWRK3_RQC) && !status) {	/* Whilst there's incoming data */
		if ((page = inb(EWRK3_RQ)) < lp->mPage) {	/* Get next entry's buffer page */
			/*
			   ** Set up shared memory window and pointer into the window
			 */
			if (lp->shmem_length == IO_ONLY) {
				outb(page, EWRK3_IOPR);
			} else if (lp->shmem_length == SHMEM_2K) {
				buf = lp->shmem;
				outb(page, EWRK3_MPR);
			} else if (lp->shmem_length == SHMEM_32K) {
				buf = (((short) page << 11) & 0x7800) + lp->shmem;
				outb((page >> 4), EWRK3_MPR);
			} else if (lp->shmem_length == SHMEM_64K) {
				buf = (((short) page << 11) & 0xf800) + lp->shmem;
				outb((page >> 5), EWRK3_MPR);
			} else {
				status = -1;
				printk("%s: Oops - your private data area is hosed!\n", dev->name);
			}

			if (!status) {
				char rx_status;
				int pkt_len;

				if (lp->shmem_length == IO_ONLY) {
					rx_status = inb(EWRK3_DATA);
					pkt_len = inb(EWRK3_DATA);
					pkt_len |= ((u_short) inb(EWRK3_DATA) << 8);
				} else {
					rx_status = readb(buf);
					buf += 1;
					pkt_len = readw(buf);
					buf += 3;
				}

				if (!(rx_status & R_ROK)) {	/* There was an error. */
					dev->stats.rx_errors++;	/* Update the error stats. */
					if (rx_status & R_DBE)
						dev->stats.rx_frame_errors++;
					if (rx_status & R_CRC)
						dev->stats.rx_crc_errors++;
					if (rx_status & R_PLL)
						dev->stats.rx_fifo_errors++;
				} else {
					struct sk_buff *skb;

					if ((skb = dev_alloc_skb(pkt_len + 2)) != NULL) {
						unsigned char *p;
						skb_reserve(skb, 2);	/* Align to 16 bytes */
						p = skb_put(skb, pkt_len);

						if (lp->shmem_length == IO_ONLY) {
							*p = inb(EWRK3_DATA);	/* dummy read */
							for (i = 0; i < pkt_len; i++) {
								*p++ = inb(EWRK3_DATA);
							}
						} else {
							memcpy_fromio(p, buf, pkt_len);
						}

						for (i = 1; i < EWRK3_PKT_STAT_SZ - 1; i++) {
							if (pkt_len < i * EWRK3_PKT_BIN_SZ) {
								lp->pktStats.bins[i]++;
								i = EWRK3_PKT_STAT_SZ;
							}
						}
						p = skb->data;	/* Look at the dest addr */
						if (p[0] & 0x01) {	/* Multicast/Broadcast */
							if ((*(s16 *) & p[0] == -1) && (*(s16 *) & p[2] == -1) && (*(s16 *) & p[4] == -1)) {
								lp->pktStats.broadcast++;
							} else {
								lp->pktStats.multicast++;
							}
						} else if ((*(s16 *) & p[0] == *(s16 *) & dev->dev_addr[0]) &&
							   (*(s16 *) & p[2] == *(s16 *) & dev->dev_addr[2]) &&
							   (*(s16 *) & p[4] == *(s16 *) & dev->dev_addr[4])) {
							lp->pktStats.unicast++;
						}
						lp->pktStats.bins[0]++;		/* Duplicates stats.rx_packets */
						if (lp->pktStats.bins[0] == 0) {	/* Reset counters */
							memset(&lp->pktStats, 0, sizeof(lp->pktStats));
						}
						/*
						   ** Notify the upper protocol layers that there is another
						   ** packet to handle
						 */
						skb->protocol = eth_type_trans(skb, dev);
						netif_rx(skb);

						/*
						   ** Update stats
						 */
						dev->stats.rx_packets++;
						dev->stats.rx_bytes += pkt_len;
					} else {
						printk("%s: Insufficient memory; nuking packet.\n", dev->name);
						dev->stats.rx_dropped++;		/* Really, deferred. */
						break;
					}
				}
			}
			/*
			   ** Return the received buffer to the free memory queue
			 */
			outb(page, EWRK3_FMQ);
		} else {
			printk("ewrk3_rx(): Illegal page number, page %d\n", page);
			printk("ewrk3_rx(): CSR: %02x ICR: %02x FMQC: %02x\n", inb(EWRK3_CSR), inb(EWRK3_ICR), inb(EWRK3_FMQC));
		}
	}
	return status;
}

static int ewrk3_tx(struct net_device *dev)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	u_long iobase = dev->base_addr;
	u_char tx_status;

	while ((tx_status = inb(EWRK3_TDQ)) > 0) {	/* Whilst there's old buffers */
		if (tx_status & T_VSTS) {	/* The status is valid */
			if (tx_status & T_TXE) {
				dev->stats.tx_errors++;
				if (tx_status & T_NCL)
					dev->stats.tx_carrier_errors++;
				if (tx_status & T_LCL)
					dev->stats.tx_window_errors++;
				if (tx_status & T_CTU) {
					if ((tx_status & T_COLL) ^ T_XUR) {
						lp->pktStats.tx_underruns++;
					} else {
						lp->pktStats.excessive_underruns++;
					}
				} else if (tx_status & T_COLL) {
					if ((tx_status & T_COLL) ^ T_XCOLL) {
						dev->stats.collisions++;
					} else {
						lp->pktStats.excessive_collisions++;
					}
				}
			} else {
				dev->stats.tx_packets++;
			}
		}
	}

	return 0;
}

static int ewrk3_close(struct net_device *dev)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	u_long iobase = dev->base_addr;
	u_char icr, csr;

	netif_stop_queue(dev);

	if (ewrk3_debug > 1) {
		printk("%s: Shutting down ethercard, status was %2.2x.\n",
		       dev->name, inb(EWRK3_CSR));
	}
	/*
	   ** We stop the EWRK3 here... mask interrupts and stop TX & RX
	 */
	DISABLE_IRQs;

	STOP_EWRK3;

	/*
	   ** Clean out the TX and RX queues here (note that one entry
	   ** may get added to either the TXD or RX queues if the TX or RX
	   ** just starts processing a packet before the STOP_EWRK3 command
	   ** is received. This will be flushed in the ewrk3_open() call).
	 */
	while (inb(EWRK3_TQ));
	while (inb(EWRK3_TDQ));
	while (inb(EWRK3_RQ));

	if (!lp->hard_strapped) {
		free_irq(dev->irq, dev);
	}
	return 0;
}

static void set_multicast_list(struct net_device *dev)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	u_long iobase = dev->base_addr;
	u_char csr;

	csr = inb(EWRK3_CSR);

	if (lp->shmem_length == IO_ONLY) {
		lp->mctbl = NULL;
	} else {
		lp->mctbl = lp->shmem + PAGE0_HTE;
	}

	csr &= ~(CSR_PME | CSR_MCE);
	if (dev->flags & IFF_PROMISC) {		/* set promiscuous mode */
		csr |= CSR_PME;
		outb(csr, EWRK3_CSR);
	} else {
		SetMulticastFilter(dev);
		csr |= CSR_MCE;
		outb(csr, EWRK3_CSR);
	}
}

static void SetMulticastFilter(struct net_device *dev)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	struct dev_mc_list *dmi = dev->mc_list;
	u_long iobase = dev->base_addr;
	int i;
	char *addrs, bit, byte;
	short __iomem *p = lp->mctbl;
	u16 hashcode;
	u32 crc;

	spin_lock_irq(&lp->hw_lock);

	if (lp->shmem_length == IO_ONLY) {
		outb(0, EWRK3_IOPR);
		outw(PAGE0_HTE, EWRK3_PIR1);
	} else {
		outb(0, EWRK3_MPR);
	}

	if (dev->flags & IFF_ALLMULTI) {
		for (i = 0; i < (HASH_TABLE_LEN >> 3); i++) {
			if (lp->shmem_length == IO_ONLY) {
				outb(0xff, EWRK3_DATA);
			} else {	/* memset didn't work here */
				writew(0xffff, p);
				p++;
				i++;
			}
		}
	} else {
		/* Clear table except for broadcast bit */
		if (lp->shmem_length == IO_ONLY) {
			for (i = 0; i < (HASH_TABLE_LEN >> 4) - 1; i++) {
				outb(0x00, EWRK3_DATA);
			}
			outb(0x80, EWRK3_DATA);
			i++;	/* insert the broadcast bit */
			for (; i < (HASH_TABLE_LEN >> 3); i++) {
				outb(0x00, EWRK3_DATA);
			}
		} else {
			memset_io(lp->mctbl, 0, HASH_TABLE_LEN >> 3);
			writeb(0x80, lp->mctbl + (HASH_TABLE_LEN >> 4) - 1);
		}

		/* Update table */
		for (i = 0; i < dev->mc_count; i++) {	/* for each address in the list */
			addrs = dmi->dmi_addr;
			dmi = dmi->next;
			if ((*addrs & 0x01) == 1) {	/* multicast address? */
				crc = ether_crc_le(ETH_ALEN, addrs);
				hashcode = crc & ((1 << 9) - 1);	/* hashcode is 9 LSb of CRC */

				byte = hashcode >> 3;	/* bit[3-8] -> byte in filter */
				bit = 1 << (hashcode & 0x07);	/* bit[0-2] -> bit in byte */

				if (lp->shmem_length == IO_ONLY) {
					u_char tmp;

					outw(PAGE0_HTE + byte, EWRK3_PIR1);
					tmp = inb(EWRK3_DATA);
					tmp |= bit;
					outw(PAGE0_HTE + byte, EWRK3_PIR1);
					outb(tmp, EWRK3_DATA);
				} else {
					writeb(readb(lp->mctbl + byte) | bit, lp->mctbl + byte);
				}
			}
		}
	}

	spin_unlock_irq(&lp->hw_lock);
}

static int __init isa_probe(struct net_device *dev, u_long ioaddr)
{
	int i = num_ewrks3s, maxSlots;
	int ret = -ENODEV;

	u_long iobase;

	if (ioaddr >= 0x400)
		goto out;

	if (ioaddr == 0) {	/* Autoprobing */
		iobase = EWRK3_IO_BASE;		/* Get the first slot address */
		maxSlots = 24;
	} else {		/* Probe a specific location */
		iobase = ioaddr;
		maxSlots = i + 1;
	}

	for (; (i < maxSlots) && (dev != NULL);
	     iobase += EWRK3_IOP_INC, i++)
	{
		if (request_region(iobase, EWRK3_TOTAL_SIZE, DRV_NAME)) {
			if (DevicePresent(iobase) == 0) {
				int irq = dev->irq;
				ret = ewrk3_hw_init(dev, iobase);
				if (!ret)
					break;
				dev->irq = irq;
			}
			release_region(iobase, EWRK3_TOTAL_SIZE);
		}
	}
 out:

	return ret;
}

static int __init eisa_probe(struct net_device *dev, u_long ioaddr)
{
	int i, maxSlots;
	u_long iobase;
	int ret = -ENODEV;

	if (ioaddr < 0x1000)
		goto out;

	iobase = ioaddr;
	i = (ioaddr >> 12);
	maxSlots = i + 1;

	for (i = 1; (i < maxSlots) && (dev != NULL); i++, iobase += EISA_SLOT_INC) {
		if (EISA_signature(name, EISA_ID) == 0) {
			if (request_region(iobase, EWRK3_TOTAL_SIZE, DRV_NAME) &&
			    DevicePresent(iobase) == 0) {
				int irq = dev->irq;
				ret = ewrk3_hw_init(dev, iobase);
				if (!ret)
					break;
				dev->irq = irq;
			}
			release_region(iobase, EWRK3_TOTAL_SIZE);
		}
	}

 out:
	return ret;
}


static int Read_EEPROM(u_long iobase, u_char eaddr)
{
	int i;

	outb((eaddr & 0x3f), EWRK3_PIR1);	/* set up 6 bits of address info */
	outb(EEPROM_RD, EWRK3_IOPR);	/* issue read command */
	for (i = 0; i < 5000; i++)
		inb(EWRK3_CSR);	/* wait 1msec */

	return inw(EWRK3_EPROM1);	/* 16 bits data return */
}

static int Write_EEPROM(short data, u_long iobase, u_char eaddr)
{
	int i;

	outb(EEPROM_WR_EN, EWRK3_IOPR);		/* issue write enable command */
	for (i = 0; i < 5000; i++)
		inb(EWRK3_CSR);	/* wait 1msec */
	outw(data, EWRK3_EPROM1);	/* write data to register */
	outb((eaddr & 0x3f), EWRK3_PIR1);	/* set up 6 bits of address info */
	outb(EEPROM_WR, EWRK3_IOPR);	/* issue write command */
	for (i = 0; i < 75000; i++)
		inb(EWRK3_CSR);	/* wait 15msec */
	outb(EEPROM_WR_DIS, EWRK3_IOPR);	/* issue write disable command */
	for (i = 0; i < 5000; i++)
		inb(EWRK3_CSR);	/* wait 1msec */

	return 0;
}

static void __init EthwrkSignature(char *name, char *eeprom_image)
{
	int i;
	char *signatures[] = EWRK3_SIGNATURE;

	for (i=0; *signatures[i] != '\0'; i++)
		if( !strncmp(eeprom_image+EEPROM_PNAME7, signatures[i], strlen(signatures[i])) )
			break;

	if (*signatures[i] != '\0') {
		memcpy(name, eeprom_image+EEPROM_PNAME7, EWRK3_STRLEN);
		name[EWRK3_STRLEN] = '\0';
	} else
		name[0] = '\0';

	return;
}


static int __init DevicePresent(u_long iobase)
{
	union {
		struct {
			u32 a;
			u32 b;
		} llsig;
		char Sig[sizeof(u32) << 1];
	}
	dev;
	short sigLength;
	char data;
	int i, j, status = 0;

	dev.llsig.a = ETH_PROM_SIG;
	dev.llsig.b = ETH_PROM_SIG;
	sigLength = sizeof(u32) << 1;

	for (i = 0, j = 0; j < sigLength && i < PROBE_LENGTH + sigLength - 1; i++) {
		data = inb(EWRK3_APROM);
		if (dev.Sig[j] == data) {	/* track signature */
			j++;
		} else {	/* lost signature; begin search again */
			if (data == dev.Sig[0]) {
				j = 1;
			} else {
				j = 0;
			}
		}
	}

	if (j != sigLength) {
		status = -ENODEV;	/* search failed */
	}
	return status;
}

static u_char __init get_hw_addr(struct net_device *dev, u_char * eeprom_image, char chipType)
{
	int i, j, k;
	u_short chksum;
	u_char crc, lfsr, sd, status = 0;
	u_long iobase = dev->base_addr;
	u16 tmp;

	if (chipType == LeMAC2) {
		for (crc = 0x6a, j = 0; j < ETH_ALEN; j++) {
			sd = dev->dev_addr[j] = eeprom_image[EEPROM_PADDR0 + j];
			outb(dev->dev_addr[j], EWRK3_PAR0 + j);
			for (k = 0; k < 8; k++, sd >>= 1) {
				lfsr = ((((crc & 0x02) >> 1) ^ (crc & 0x01)) ^ (sd & 0x01)) << 7;
				crc = (crc >> 1) + lfsr;
			}
		}
		if (crc != eeprom_image[EEPROM_PA_CRC])
			status = -1;
	} else {
		for (i = 0, k = 0; i < ETH_ALEN;) {
			k <<= 1;
			if (k > 0xffff)
				k -= 0xffff;

			k += (u_char) (tmp = inb(EWRK3_APROM));
			dev->dev_addr[i] = (u_char) tmp;
			outb(dev->dev_addr[i], EWRK3_PAR0 + i);
			i++;
			k += (u_short) ((tmp = inb(EWRK3_APROM)) << 8);
			dev->dev_addr[i] = (u_char) tmp;
			outb(dev->dev_addr[i], EWRK3_PAR0 + i);
			i++;

			if (k > 0xffff)
				k -= 0xffff;
		}
		if (k == 0xffff)
			k = 0;
		chksum = inb(EWRK3_APROM);
		chksum |= (inb(EWRK3_APROM) << 8);
		if (k != chksum)
			status = -1;
	}

	return status;
}

static int __init EISA_signature(char *name, s32 eisa_id)
{
	u_long i;
	char *signatures[] = EWRK3_SIGNATURE;
	char ManCode[EWRK3_STRLEN];
	union {
		s32 ID;
		char Id[4];
	} Eisa;
	int status = 0;

	*name = '\0';
	for (i = 0; i < 4; i++) {
		Eisa.Id[i] = inb(eisa_id + i);
	}

	ManCode[0] = (((Eisa.Id[0] >> 2) & 0x1f) + 0x40);
	ManCode[1] = (((Eisa.Id[1] & 0xe0) >> 5) + ((Eisa.Id[0] & 0x03) << 3) + 0x40);
	ManCode[2] = (((Eisa.Id[2] >> 4) & 0x0f) + 0x30);
	ManCode[3] = ((Eisa.Id[2] & 0x0f) + 0x30);
	ManCode[4] = (((Eisa.Id[3] >> 4) & 0x0f) + 0x30);
	ManCode[5] = '\0';

	for (i = 0; (*signatures[i] != '\0') && (*name == '\0'); i++) {
		if (strstr(ManCode, signatures[i]) != NULL) {
			strcpy(name, ManCode);
			status = 1;
		}
	}

	return status;		/* return the device name string */
}

static void ewrk3_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	int fwrev = Read_EEPROM(dev->base_addr, EEPROM_REVLVL);

	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	sprintf(info->fw_version, "%d", fwrev);
	strcpy(info->bus_info, "N/A");
	info->eedump_len = EEPROM_MAX;
}

static int ewrk3_get_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	unsigned long iobase = dev->base_addr;
	u8 cr = inb(EWRK3_CR);

	switch (lp->adapter_name[4]) {
	case '3': /* DE203 */
		ecmd->supported = SUPPORTED_BNC;
		ecmd->port = PORT_BNC;
		break;

	case '4': /* DE204 */
		ecmd->supported = SUPPORTED_TP;
		ecmd->port = PORT_TP;
		break;

	case '5': /* DE205 */
		ecmd->supported = SUPPORTED_TP | SUPPORTED_BNC | SUPPORTED_AUI;
		ecmd->autoneg = !(cr & CR_APD);
		/*
		** Port is only valid if autoneg is disabled
		** and even then we don't know if AUI is jumpered.
		*/
		if (!ecmd->autoneg)
			ecmd->port = (cr & CR_PSEL) ? PORT_BNC : PORT_TP;
		break;
	}

	ecmd->supported |= SUPPORTED_10baseT_Half;
	ecmd->speed = SPEED_10;
	ecmd->duplex = DUPLEX_HALF;
	return 0;
}

static int ewrk3_set_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	unsigned long iobase = dev->base_addr;
	unsigned long flags;
	u8 cr;

	/* DE205 is the only card with anything to set */
	if (lp->adapter_name[4] != '5')
		return -EOPNOTSUPP;

	/* Sanity-check parameters */
	if (ecmd->speed != SPEED_10)
		return -EINVAL;
	if (ecmd->port != PORT_TP && ecmd->port != PORT_BNC)
		return -EINVAL; /* AUI is not software-selectable */
	if (ecmd->transceiver != XCVR_INTERNAL)
		return -EINVAL;
	if (ecmd->duplex != DUPLEX_HALF)
		return -EINVAL;
	if (ecmd->phy_address != 0)
		return -EINVAL;

	spin_lock_irqsave(&lp->hw_lock, flags);
	cr = inb(EWRK3_CR);

	/* If Autoneg is set, change to Auto Port mode */
	/* Otherwise, disable Auto Port and set port explicitly */
	if (ecmd->autoneg) {
		cr &= ~CR_APD;
	} else {
		cr |= CR_APD;
		if (ecmd->port == PORT_TP)
			cr &= ~CR_PSEL;		/* Force TP */
		else
			cr |= CR_PSEL;		/* Force BNC */
	}

	/* Commit the changes */
	outb(cr, EWRK3_CR);
	spin_unlock_irqrestore(&lp->hw_lock, flags);
	return 0;
}

static u32 ewrk3_get_link(struct net_device *dev)
{
	unsigned long iobase = dev->base_addr;
	u8 cmr = inb(EWRK3_CMR);
	/* DE203 has BNC only and link status does not apply */
	/* On DE204 this is always valid since TP is the only port. */
	/* On DE205 this reflects TP status even if BNC or AUI is selected. */
	return !(cmr & CMR_LINK);
}

static int ewrk3_phys_id(struct net_device *dev, u32 data)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	unsigned long iobase = dev->base_addr;
	unsigned long flags;
	u8 cr;
	int count;

	/* Toggle LED 4x per second */
	count = data << 2;

	spin_lock_irqsave(&lp->hw_lock, flags);

	/* Bail if a PHYS_ID is already in progress */
	if (lp->led_mask == 0) {
		spin_unlock_irqrestore(&lp->hw_lock, flags);
		return -EBUSY;
	}

	/* Prevent ISR from twiddling the LED */
	lp->led_mask = 0;

	while (count--) {
		/* Toggle the LED */
		cr = inb(EWRK3_CR);
		outb(cr ^ CR_LED, EWRK3_CR);

		/* Wait a little while */
		spin_unlock_irqrestore(&lp->hw_lock, flags);
		msleep(250);
		spin_lock_irqsave(&lp->hw_lock, flags);

		/* Exit if we got a signal */
		if (signal_pending(current))
			break;
	}

	lp->led_mask = CR_LED;
	cr = inb(EWRK3_CR);
	outb(cr & ~CR_LED, EWRK3_CR);
	spin_unlock_irqrestore(&lp->hw_lock, flags);
	return signal_pending(current) ? -ERESTARTSYS : 0;
}

static const struct ethtool_ops ethtool_ops_203 = {
	.get_drvinfo = ewrk3_get_drvinfo,
	.get_settings = ewrk3_get_settings,
	.set_settings = ewrk3_set_settings,
	.phys_id = ewrk3_phys_id,
};

static const struct ethtool_ops ethtool_ops = {
	.get_drvinfo = ewrk3_get_drvinfo,
	.get_settings = ewrk3_get_settings,
	.set_settings = ewrk3_set_settings,
	.get_link = ewrk3_get_link,
	.phys_id = ewrk3_phys_id,
};

static int ewrk3_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct ewrk3_private *lp = netdev_priv(dev);
	struct ewrk3_ioctl *ioc = (struct ewrk3_ioctl *) &rq->ifr_ifru;
	u_long iobase = dev->base_addr;
	int i, j, status = 0;
	u_char csr;
	unsigned long flags;
	union ewrk3_addr {
		u_char addr[HASH_TABLE_LEN * ETH_ALEN];
		u_short val[(HASH_TABLE_LEN * ETH_ALEN) >> 1];
	};

	union ewrk3_addr *tmp;

	/* All we handle are private IOCTLs */
	if (cmd != EWRK3IOCTL)
		return -EOPNOTSUPP;

	tmp = kmalloc(sizeof(union ewrk3_addr), GFP_KERNEL);
	if(tmp==NULL)
		return -ENOMEM;

	switch (ioc->cmd) {
	case EWRK3_GET_HWADDR:	/* Get the hardware address */
		for (i = 0; i < ETH_ALEN; i++) {
			tmp->addr[i] = dev->dev_addr[i];
		}
		ioc->len = ETH_ALEN;
		if (copy_to_user(ioc->data, tmp->addr, ioc->len))
			status = -EFAULT;
		break;

	case EWRK3_SET_HWADDR:	/* Set the hardware address */
		if (capable(CAP_NET_ADMIN)) {
			spin_lock_irqsave(&lp->hw_lock, flags);
			csr = inb(EWRK3_CSR);
			csr |= (CSR_TXD | CSR_RXD);
			outb(csr, EWRK3_CSR);	/* Disable the TX and RX */
			spin_unlock_irqrestore(&lp->hw_lock, flags);

			if (copy_from_user(tmp->addr, ioc->data, ETH_ALEN)) {
				status = -EFAULT;
				break;
			}
			spin_lock_irqsave(&lp->hw_lock, flags);
			for (i = 0; i < ETH_ALEN; i++) {
				dev->dev_addr[i] = tmp->addr[i];
				outb(tmp->addr[i], EWRK3_PAR0 + i);
			}

			csr = inb(EWRK3_CSR);
			csr &= ~(CSR_TXD | CSR_RXD);	/* Enable the TX and RX */
			outb(csr, EWRK3_CSR);
			spin_unlock_irqrestore(&lp->hw_lock, flags);
		} else {
			status = -EPERM;
		}

		break;
	case EWRK3_SET_PROM:	/* Set Promiscuous Mode */
		if (capable(CAP_NET_ADMIN)) {
			spin_lock_irqsave(&lp->hw_lock, flags);
			csr = inb(EWRK3_CSR);
			csr |= CSR_PME;
			csr &= ~CSR_MCE;
			outb(csr, EWRK3_CSR);
			spin_unlock_irqrestore(&lp->hw_lock, flags);
		} else {
			status = -EPERM;
		}

		break;
	case EWRK3_CLR_PROM:	/* Clear Promiscuous Mode */
		if (capable(CAP_NET_ADMIN)) {
			spin_lock_irqsave(&lp->hw_lock, flags);
			csr = inb(EWRK3_CSR);
			csr &= ~CSR_PME;
			outb(csr, EWRK3_CSR);
			spin_unlock_irqrestore(&lp->hw_lock, flags);
		} else {
			status = -EPERM;
		}

		break;
	case EWRK3_GET_MCA:	/* Get the multicast address table */
		spin_lock_irqsave(&lp->hw_lock, flags);
		if (lp->shmem_length == IO_ONLY) {
			outb(0, EWRK3_IOPR);
			outw(PAGE0_HTE, EWRK3_PIR1);
			for (i = 0; i < (HASH_TABLE_LEN >> 3); i++) {
				tmp->addr[i] = inb(EWRK3_DATA);
			}
		} else {
			outb(0, EWRK3_MPR);
			memcpy_fromio(tmp->addr, lp->shmem + PAGE0_HTE, (HASH_TABLE_LEN >> 3));
		}
		spin_unlock_irqrestore(&lp->hw_lock, flags);

		ioc->len = (HASH_TABLE_LEN >> 3);
		if (copy_to_user(ioc->data, tmp->addr, ioc->len))
			status = -EFAULT;

		break;
	case EWRK3_SET_MCA:	/* Set a multicast address */
		if (capable(CAP_NET_ADMIN)) {
			if (ioc->len > 1024)
			{
				status = -EINVAL;
				break;
			}
			if (copy_from_user(tmp->addr, ioc->data, ETH_ALEN * ioc->len)) {
				status = -EFAULT;
				break;
			}
			set_multicast_list(dev);
		} else {
			status = -EPERM;
		}

		break;
	case EWRK3_CLR_MCA:	/* Clear all multicast addresses */
		if (capable(CAP_NET_ADMIN)) {
			set_multicast_list(dev);
		} else {
			status = -EPERM;
		}

		break;
	case EWRK3_MCA_EN:	/* Enable multicast addressing */
		if (capable(CAP_NET_ADMIN)) {
			spin_lock_irqsave(&lp->hw_lock, flags);
			csr = inb(EWRK3_CSR);
			csr |= CSR_MCE;
			csr &= ~CSR_PME;
			outb(csr, EWRK3_CSR);
			spin_unlock_irqrestore(&lp->hw_lock, flags);
		} else {
			status = -EPERM;
		}

		break;
	case EWRK3_GET_STATS: { /* Get the driver statistics */
		struct ewrk3_stats *tmp_stats =
        		kmalloc(sizeof(lp->pktStats), GFP_KERNEL);
		if (!tmp_stats) {
			status = -ENOMEM;
			break;
		}

		spin_lock_irqsave(&lp->hw_lock, flags);
		memcpy(tmp_stats, &lp->pktStats, sizeof(lp->pktStats));
		spin_unlock_irqrestore(&lp->hw_lock, flags);

		ioc->len = sizeof(lp->pktStats);
		if (copy_to_user(ioc->data, tmp_stats, sizeof(lp->pktStats)))
    			status = -EFAULT;
		kfree(tmp_stats);
		break;
	}
	case EWRK3_CLR_STATS:	/* Zero out the driver statistics */
		if (capable(CAP_NET_ADMIN)) {
			spin_lock_irqsave(&lp->hw_lock, flags);
			memset(&lp->pktStats, 0, sizeof(lp->pktStats));
			spin_unlock_irqrestore(&lp->hw_lock,flags);
		} else {
			status = -EPERM;
		}

		break;
	case EWRK3_GET_CSR:	/* Get the CSR Register contents */
		tmp->addr[0] = inb(EWRK3_CSR);
		ioc->len = 1;
		if (copy_to_user(ioc->data, tmp->addr, ioc->len))
			status = -EFAULT;
		break;
	case EWRK3_SET_CSR:	/* Set the CSR Register contents */
		if (capable(CAP_NET_ADMIN)) {
			if (copy_from_user(tmp->addr, ioc->data, 1)) {
				status = -EFAULT;
				break;
			}
			outb(tmp->addr[0], EWRK3_CSR);
		} else {
			status = -EPERM;
		}

		break;
	case EWRK3_GET_EEPROM:	/* Get the EEPROM contents */
		if (capable(CAP_NET_ADMIN)) {
			for (i = 0; i < (EEPROM_MAX >> 1); i++) {
				tmp->val[i] = (short) Read_EEPROM(iobase, i);
			}
			i = EEPROM_MAX;
			tmp->addr[i++] = inb(EWRK3_CMR);		/* Config/Management Reg. */
			for (j = 0; j < ETH_ALEN; j++) {
				tmp->addr[i++] = inb(EWRK3_PAR0 + j);
			}
			ioc->len = EEPROM_MAX + 1 + ETH_ALEN;
			if (copy_to_user(ioc->data, tmp->addr, ioc->len))
				status = -EFAULT;
		} else {
			status = -EPERM;
		}

		break;
	case EWRK3_SET_EEPROM:	/* Set the EEPROM contents */
		if (capable(CAP_NET_ADMIN)) {
			if (copy_from_user(tmp->addr, ioc->data, EEPROM_MAX)) {
				status = -EFAULT;
				break;
			}
			for (i = 0; i < (EEPROM_MAX >> 1); i++) {
				Write_EEPROM(tmp->val[i], iobase, i);
			}
		} else {
			status = -EPERM;
		}

		break;
	case EWRK3_GET_CMR:	/* Get the CMR Register contents */
		tmp->addr[0] = inb(EWRK3_CMR);
		ioc->len = 1;
		if (copy_to_user(ioc->data, tmp->addr, ioc->len))
			status = -EFAULT;
		break;
	case EWRK3_SET_TX_CUT_THRU:	/* Set TX cut through mode */
		if (capable(CAP_NET_ADMIN)) {
			lp->txc = 1;
		} else {
			status = -EPERM;
		}

		break;
	case EWRK3_CLR_TX_CUT_THRU:	/* Clear TX cut through mode */
		if (capable(CAP_NET_ADMIN)) {
			lp->txc = 0;
		} else {
			status = -EPERM;
		}

		break;
	default:
		status = -EOPNOTSUPP;
	}
	kfree(tmp);
	return status;
}

#ifdef MODULE
static struct net_device *ewrk3_devs[MAX_NUM_EWRK3S];
static int ndevs;
static int io[MAX_NUM_EWRK3S+1] = { 0x300, 0, };

/* '21' below should really be 'MAX_NUM_EWRK3S' */
module_param_array(io, int, NULL, 0);
module_param_array(irq, int, NULL, 0);
MODULE_PARM_DESC(io, "EtherWORKS 3 I/O base address(es)");
MODULE_PARM_DESC(irq, "EtherWORKS 3 IRQ number(s)");

static __exit void ewrk3_exit_module(void)
{
	int i;

	for( i=0; i<ndevs; i++ ) {
		struct net_device *dev = ewrk3_devs[i];
		struct ewrk3_private *lp = netdev_priv(dev);
		ewrk3_devs[i] = NULL;
		unregister_netdev(dev);
		release_region(dev->base_addr, EWRK3_TOTAL_SIZE);
		iounmap(lp->shmem);
		free_netdev(dev);
	}
}

static __init int ewrk3_init_module(void)
{
	int i=0;

	while( io[i] && irq[i] ) {
		struct net_device *dev
			= alloc_etherdev(sizeof(struct ewrk3_private));

		if (!dev)
			break;

		if (ewrk3_probe1(dev, io[i], irq[i]) != 0) {
			free_netdev(dev);
			break;
		}

		ewrk3_devs[ndevs++] = dev;
		i++;
	}

	return ndevs ? 0 : -EIO;
}


/* Hack for breakage in new module stuff */
module_exit(ewrk3_exit_module);
module_init(ewrk3_init_module);
#endif				/* MODULE */
MODULE_LICENSE("GPL");
