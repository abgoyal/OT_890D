

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>

#define DRV_NAME	"pata_hpt3x2n"
#define DRV_VERSION	"0.3.4"

enum {
	HPT_PCI_FAST	=	(1 << 31),
	PCI66		=	(1 << 1),
	USE_DPLL	=	(1 << 0)
};

struct hpt_clock {
	u8	xfer_speed;
	u32	timing;
};

struct hpt_chip {
	const char *name;
	struct hpt_clock *clocks[3];
};


/* 66MHz DPLL clocks */

static struct hpt_clock hpt3x2n_clocks[] = {
	{	XFER_UDMA_7,	0x1c869c62	},
	{	XFER_UDMA_6,	0x1c869c62	},
	{	XFER_UDMA_5,	0x1c8a9c62	},
	{	XFER_UDMA_4,	0x1c8a9c62	},
	{	XFER_UDMA_3,	0x1c8e9c62	},
	{	XFER_UDMA_2,	0x1c929c62	},
	{	XFER_UDMA_1,	0x1c9a9c62	},
	{	XFER_UDMA_0,	0x1c829c62	},

	{	XFER_MW_DMA_2,	0x2c829c62	},
	{	XFER_MW_DMA_1,	0x2c829c66	},
	{	XFER_MW_DMA_0,	0x2c829d2c	},

	{	XFER_PIO_4,	0x0c829c62	},
	{	XFER_PIO_3,	0x0c829c84	},
	{	XFER_PIO_2,	0x0c829ca6	},
	{	XFER_PIO_1,	0x0d029d26	},
	{	XFER_PIO_0,	0x0d029d5e	},
	{	0,		0x0d029d5e	}
};


static u32 hpt3x2n_find_mode(struct ata_port *ap, int speed)
{
	struct hpt_clock *clocks = hpt3x2n_clocks;

	while(clocks->xfer_speed) {
		if (clocks->xfer_speed == speed)
			return clocks->timing;
		clocks++;
	}
	BUG();
	return 0xffffffffU;	/* silence compiler warning */
}


static int hpt3x2n_cable_detect(struct ata_port *ap)
{
	u8 scr2, ata66;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);

	pci_read_config_byte(pdev, 0x5B, &scr2);
	pci_write_config_byte(pdev, 0x5B, scr2 & ~0x01);
	/* Cable register now active */
	pci_read_config_byte(pdev, 0x5A, &ata66);
	/* Restore state */
	pci_write_config_byte(pdev, 0x5B, scr2);

	if (ata66 & (1 << ap->port_no))
		return ATA_CBL_PATA40;
	else
		return ATA_CBL_PATA80;
}


static int hpt3x2n_pre_reset(struct ata_link *link, unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	/* Reset the state machine */
	pci_write_config_byte(pdev, 0x50 + 4 * ap->port_no, 0x37);
	udelay(100);

	return ata_sff_prereset(link, deadline);
}


static void hpt3x2n_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u32 addr1, addr2;
	u32 reg;
	u32 mode;
	u8 fast;

	addr1 = 0x40 + 4 * (adev->devno + 2 * ap->port_no);
	addr2 = 0x51 + 4 * ap->port_no;

	/* Fast interrupt prediction disable, hold off interrupt disable */
	pci_read_config_byte(pdev, addr2, &fast);
	fast &= ~0x07;
	pci_write_config_byte(pdev, addr2, fast);

	pci_read_config_dword(pdev, addr1, &reg);
	mode = hpt3x2n_find_mode(ap, adev->pio_mode);
	mode &= ~0x8000000;	/* No FIFO in PIO */
	mode &= ~0x30070000;	/* Leave config bits alone */
	reg &= 0x30070000;	/* Strip timing bits */
	pci_write_config_dword(pdev, addr1, reg | mode);
}


static void hpt3x2n_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u32 addr1, addr2;
	u32 reg;
	u32 mode;
	u8 fast;

	addr1 = 0x40 + 4 * (adev->devno + 2 * ap->port_no);
	addr2 = 0x51 + 4 * ap->port_no;

	/* Fast interrupt prediction disable, hold off interrupt disable */
	pci_read_config_byte(pdev, addr2, &fast);
	fast &= ~0x07;
	pci_write_config_byte(pdev, addr2, fast);

	pci_read_config_dword(pdev, addr1, &reg);
	mode = hpt3x2n_find_mode(ap, adev->dma_mode);
	mode |= 0x8000000;	/* FIFO in MWDMA or UDMA */
	mode &= ~0xC0000000;	/* Leave config bits alone */
	reg &= 0xC0000000;	/* Strip timing bits */
	pci_write_config_dword(pdev, addr1, reg | mode);
}


static void hpt3x2n_bmdma_stop(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int mscreg = 0x50 + 2 * ap->port_no;
	u8 bwsr_stat, msc_stat;

	pci_read_config_byte(pdev, 0x6A, &bwsr_stat);
	pci_read_config_byte(pdev, mscreg, &msc_stat);
	if (bwsr_stat & (1 << ap->port_no))
		pci_write_config_byte(pdev, mscreg, msc_stat | 0x30);
	ata_bmdma_stop(qc);
}


static void hpt3x2n_set_clock(struct ata_port *ap, int source)
{
	void __iomem *bmdma = ap->ioaddr.bmdma_addr;

	/* Tristate the bus */
	iowrite8(0x80, bmdma+0x73);
	iowrite8(0x80, bmdma+0x77);

	/* Switch clock and reset channels */
	iowrite8(source, bmdma+0x7B);
	iowrite8(0xC0, bmdma+0x79);

	/* Reset state machines */
	iowrite8(0x37, bmdma+0x70);
	iowrite8(0x37, bmdma+0x74);

	/* Complete reset */
	iowrite8(0x00, bmdma+0x79);

	/* Reconnect channels to bus */
	iowrite8(0x00, bmdma+0x73);
	iowrite8(0x00, bmdma+0x77);
}

/* Check if our partner interface is busy */

static int hpt3x2n_pair_idle(struct ata_port *ap)
{
	struct ata_host *host = ap->host;
	struct ata_port *pair = host->ports[ap->port_no ^ 1];

	if (pair->hsm_task_state == HSM_ST_IDLE)
		return 1;
	return 0;
}

static int hpt3x2n_use_dpll(struct ata_port *ap, int writing)
{
	long flags = (long)ap->host->private_data;
	/* See if we should use the DPLL */
	if (writing)
		return USE_DPLL;	/* Needed for write */
	if (flags & PCI66)
		return USE_DPLL;	/* Needed at 66Mhz */
	return 0;
}

static unsigned int hpt3x2n_qc_issue(struct ata_queued_cmd *qc)
{
	struct ata_taskfile *tf = &qc->tf;
	struct ata_port *ap = qc->ap;
	int flags = (long)ap->host->private_data;

	if (hpt3x2n_pair_idle(ap)) {
		int dpll = hpt3x2n_use_dpll(ap, (tf->flags & ATA_TFLAG_WRITE));
		if ((flags & USE_DPLL) != dpll) {
			if (dpll == 1)
				hpt3x2n_set_clock(ap, 0x21);
			else
				hpt3x2n_set_clock(ap, 0x23);
		}
	}
	return ata_sff_qc_issue(qc);
}

static struct scsi_host_template hpt3x2n_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};


static struct ata_port_operations hpt3x2n_port_ops = {
	.inherits	= &ata_bmdma_port_ops,

	.bmdma_stop	= hpt3x2n_bmdma_stop,
	.qc_issue	= hpt3x2n_qc_issue,

	.cable_detect	= hpt3x2n_cable_detect,
	.set_piomode	= hpt3x2n_set_piomode,
	.set_dmamode	= hpt3x2n_set_dmamode,
	.prereset	= hpt3x2n_pre_reset,
};


static int hpt3xn_calibrate_dpll(struct pci_dev *dev)
{
	u8 reg5b;
	u32 reg5c;
	int tries;

	for(tries = 0; tries < 0x5000; tries++) {
		udelay(50);
		pci_read_config_byte(dev, 0x5b, &reg5b);
		if (reg5b & 0x80) {
			/* See if it stays set */
			for(tries = 0; tries < 0x1000; tries ++) {
				pci_read_config_byte(dev, 0x5b, &reg5b);
				/* Failed ? */
				if ((reg5b & 0x80) == 0)
					return 0;
			}
			/* Turn off tuning, we have the DPLL set */
			pci_read_config_dword(dev, 0x5c, &reg5c);
			pci_write_config_dword(dev, 0x5c, reg5c & ~ 0x100);
			return 1;
		}
	}
	/* Never went stable */
	return 0;
}

static int hpt3x2n_pci_clock(struct pci_dev *pdev)
{
	unsigned long freq;
	u32 fcnt;
	unsigned long iobase = pci_resource_start(pdev, 4);

	fcnt = inl(iobase + 0x90);	/* Not PCI readable for some chips */
	if ((fcnt >> 12) != 0xABCDE) {
		printk(KERN_WARNING "hpt3xn: BIOS clock data not set.\n");
		return 33;	/* Not BIOS set */
	}
	fcnt &= 0x1FF;

	freq = (fcnt * 77) / 192;

	/* Clamp to bands */
	if (freq < 40)
		return 33;
	if (freq < 45)
		return 40;
	if (freq < 55)
		return 50;
	return 66;
}


static int hpt3x2n_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	/* HPT372N and friends - UDMA133 */
	static const struct ata_port_info info = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = 0x1f,
		.mwdma_mask = 0x07,
		.udma_mask = ATA_UDMA6,
		.port_ops = &hpt3x2n_port_ops
	};
	const struct ata_port_info *ppi[] = { &info, NULL };

	u8 irqmask;
	u32 class_rev;

	unsigned int pci_mhz;
	unsigned int f_low, f_high;
	int adjust;
	unsigned long iobase = pci_resource_start(dev, 4);
	void *hpriv = NULL;
	int rc;

	rc = pcim_enable_device(dev);
	if (rc)
		return rc;

	pci_read_config_dword(dev, PCI_CLASS_REVISION, &class_rev);
	class_rev &= 0xFF;

	switch(dev->device) {
		case PCI_DEVICE_ID_TTI_HPT366:
			if (class_rev < 6)
				return -ENODEV;
			break;
		case PCI_DEVICE_ID_TTI_HPT371:
			if (class_rev < 2)
				return -ENODEV;
			/* 371N if rev > 1 */
			break;
		case PCI_DEVICE_ID_TTI_HPT372:
			/* 372N if rev >= 2*/
			if (class_rev < 2)
				return -ENODEV;
			break;
		case PCI_DEVICE_ID_TTI_HPT302:
			if (class_rev < 2)
				return -ENODEV;
			break;
		case PCI_DEVICE_ID_TTI_HPT372N:
			break;
		default:
			printk(KERN_ERR "pata_hpt3x2n: PCI table is bogus please report (%d).\n", dev->device);
			return -ENODEV;
	}

	/* Ok so this is a chip we support */

	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, (L1_CACHE_BYTES / 4));
	pci_write_config_byte(dev, PCI_LATENCY_TIMER, 0x78);
	pci_write_config_byte(dev, PCI_MIN_GNT, 0x08);
	pci_write_config_byte(dev, PCI_MAX_LAT, 0x08);

	pci_read_config_byte(dev, 0x5A, &irqmask);
	irqmask &= ~0x10;
	pci_write_config_byte(dev, 0x5a, irqmask);

	/*
	 * HPT371 chips physically have only one channel, the secondary one,
	 * but the primary channel registers do exist!  Go figure...
	 * So,  we manually disable the non-existing channel here
	 * (if the BIOS hasn't done this already).
	 */
	if (dev->device == PCI_DEVICE_ID_TTI_HPT371) {
		u8 mcr1;
		pci_read_config_byte(dev, 0x50, &mcr1);
		mcr1 &= ~0x04;
		pci_write_config_byte(dev, 0x50, mcr1);
	}

	/* Tune the PLL. HPT recommend using 75 for SATA, 66 for UDMA133 or
	   50 for UDMA100. Right now we always use 66 */

	pci_mhz = hpt3x2n_pci_clock(dev);

	f_low = (pci_mhz * 48) / 66;	/* PCI Mhz for 66Mhz DPLL */
	f_high = f_low + 2;		/* Tolerance */

	pci_write_config_dword(dev, 0x5C, (f_high << 16) | f_low | 0x100);
	/* PLL clock */
	pci_write_config_byte(dev, 0x5B, 0x21);

	/* Unlike the 37x we don't try jiggling the frequency */
	for(adjust = 0; adjust < 8; adjust++) {
		if (hpt3xn_calibrate_dpll(dev))
			break;
		pci_write_config_dword(dev, 0x5C, (f_high << 16) | f_low);
	}
	if (adjust == 8) {
		printk(KERN_ERR "pata_hpt3x2n: DPLL did not stabilize!\n");
		return -ENODEV;
	}

	printk(KERN_INFO "pata_hpt37x: bus clock %dMHz, using 66MHz DPLL.\n",
	       pci_mhz);
	/* Set our private data up. We only need a few flags so we use
	   it directly */
	if (pci_mhz > 60) {
		hpriv = (void *)PCI66;
		/*
		 * On  HPT371N, if ATA clock is 66 MHz we must set bit 2 in
		 * the MISC. register to stretch the UltraDMA Tss timing.
		 * NOTE: This register is only writeable via I/O space.
		 */
		if (dev->device == PCI_DEVICE_ID_TTI_HPT371)
			outb(inb(iobase + 0x9c) | 0x04, iobase + 0x9c);
	}

	/* Now kick off ATA set up */
	return ata_pci_sff_init_one(dev, ppi, &hpt3x2n_sht, hpriv);
}

static const struct pci_device_id hpt3x2n[] = {
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT366), },
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT371), },
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT372), },
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT302), },
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT372N), },

	{ },
};

static struct pci_driver hpt3x2n_pci_driver = {
	.name 		= DRV_NAME,
	.id_table	= hpt3x2n,
	.probe 		= hpt3x2n_init_one,
	.remove		= ata_pci_remove_one
};

static int __init hpt3x2n_init(void)
{
	return pci_register_driver(&hpt3x2n_pci_driver);
}

static void __exit hpt3x2n_exit(void)
{
	pci_unregister_driver(&hpt3x2n_pci_driver);
}

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("low-level driver for the Highpoint HPT3x2n/30x");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, hpt3x2n);
MODULE_VERSION(DRV_VERSION);

module_init(hpt3x2n_init);
module_exit(hpt3x2n_exit);
