

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>

#define DRV_NAME	"sc1200"
#define DRV_VERSION	"0.2.6"

#define SC1200_REV_A	0x00
#define SC1200_REV_B1	0x01
#define SC1200_REV_B3	0x02
#define SC1200_REV_C1	0x03
#define SC1200_REV_D1	0x04


static int sc1200_clock(void)
{
	/* Magic registers that give us the chipset data */
	u8 chip_id = inb(0x903C);
	u8 silicon_rev = inb(0x903D);
	u16 pci_clock;

	if (chip_id == 0x04 && silicon_rev < SC1200_REV_B1)
		return 0;	/* 33 MHz mode */

	/* Clock generator configuration 0x901E its 8/9 are the PCI clocking
	   0/3 is 33Mhz 1 is 48 2 is 66 */

	pci_clock = inw(0x901E);
	pci_clock >>= 8;
	pci_clock &= 0x03;
	if (pci_clock == 3)
		pci_clock = 0;
	return pci_clock;
}


static void sc1200_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	static const u32 pio_timings[4][5] = {
		{0x00009172, 0x00012171, 0x00020080, 0x00032010, 0x00040010},	// format0  33Mhz
		{0xd1329172, 0x71212171, 0x30200080, 0x20102010, 0x00100010},	// format1, 33Mhz
		{0xfaa3f4f3, 0xc23232b2, 0x513101c1, 0x31213121, 0x10211021},	// format1, 48Mhz
		{0xfff4fff4, 0xf35353d3, 0x814102f1, 0x42314231, 0x11311131}	// format1, 66Mhz
	};

	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u32 format;
	unsigned int reg = 0x40 + 0x10 * ap->port_no;
	int mode = adev->pio_mode - XFER_PIO_0;

	pci_read_config_dword(pdev, reg + 4, &format);
	format >>= 31;
	format += sc1200_clock();
	pci_write_config_dword(pdev, reg + 8 * adev->devno,
				pio_timings[format][mode]);
}


static void sc1200_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	static const u32 udma_timing[3][3] = {
		{ 0x00921250, 0x00911140, 0x00911030 },
		{ 0x00932470, 0x00922260, 0x00922140 },
		{ 0x009436A1, 0x00933481, 0x00923261 }
	};

	static const u32 mwdma_timing[3][3] = {
		{ 0x00077771, 0x00012121, 0x00002020 },
		{ 0x000BBBB2, 0x00024241, 0x00013131 },
		{ 0x000FFFF3, 0x00035352, 0x00015151 }
	};

	int clock = sc1200_clock();
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	unsigned int reg = 0x40 + 0x10 * ap->port_no;
	int mode = adev->dma_mode;
	u32 format;

	if (mode >= XFER_UDMA_0)
		format = udma_timing[clock][mode - XFER_UDMA_0];
	else
		format = mwdma_timing[clock][mode - XFER_MW_DMA_0];

	if (adev->devno == 0) {
		u32 timings;

		pci_read_config_dword(pdev, reg + 4, &timings);
		timings &= 0x80000000UL;
		timings |= format;
		pci_write_config_dword(pdev, reg + 4, timings);
	} else
		pci_write_config_dword(pdev, reg + 12, format);
}


static unsigned int sc1200_qc_issue(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ata_device *adev = qc->dev;
	struct ata_device *prev = ap->private_data;

	/* See if the DMA settings could be wrong */
	if (ata_dma_enabled(adev) && adev != prev && prev != NULL) {
		/* Maybe, but do the channels match MWDMA/UDMA ? */
		if ((ata_using_udma(adev) && !ata_using_udma(prev)) ||
		    (ata_using_udma(prev) && !ata_using_udma(adev)))
		    	/* Switch the mode bits */
		    	sc1200_set_dmamode(ap, adev);
	}

	return ata_sff_qc_issue(qc);
}

static struct scsi_host_template sc1200_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
	.sg_tablesize	= LIBATA_DUMB_MAX_PRD,
};

static struct ata_port_operations sc1200_port_ops = {
	.inherits	= &ata_bmdma_port_ops,
	.qc_prep 	= ata_sff_dumb_qc_prep,
	.qc_issue	= sc1200_qc_issue,
	.cable_detect	= ata_cable_40wire,
	.set_piomode	= sc1200_set_piomode,
	.set_dmamode	= sc1200_set_dmamode,
};


static int sc1200_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	static const struct ata_port_info info = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = 0x1f,
		.mwdma_mask = 0x07,
		.udma_mask = 0x07,
		.port_ops = &sc1200_port_ops
	};
	/* Can't enable port 2 yet, see top comments */
	const struct ata_port_info *ppi[] = { &info, &ata_dummy_port_info };

	return ata_pci_sff_init_one(dev, ppi, &sc1200_sht, NULL);
}

static const struct pci_device_id sc1200[] = {
	{ PCI_VDEVICE(NS, PCI_DEVICE_ID_NS_SCx200_IDE), },

	{ },
};

static struct pci_driver sc1200_pci_driver = {
	.name 		= DRV_NAME,
	.id_table	= sc1200,
	.probe 		= sc1200_init_one,
	.remove		= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend	= ata_pci_device_suspend,
	.resume		= ata_pci_device_resume,
#endif
};

static int __init sc1200_init(void)
{
	return pci_register_driver(&sc1200_pci_driver);
}

static void __exit sc1200_exit(void)
{
	pci_unregister_driver(&sc1200_pci_driver);
}

MODULE_AUTHOR("Alan Cox, Mark Lord");
MODULE_DESCRIPTION("low-level driver for the NS/AMD SC1200");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, sc1200);
MODULE_VERSION(DRV_VERSION);

module_init(sc1200_init);
module_exit(sc1200_exit);
