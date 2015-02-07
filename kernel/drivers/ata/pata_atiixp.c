

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>

#define DRV_NAME "pata_atiixp"
#define DRV_VERSION "0.4.6"

enum {
	ATIIXP_IDE_PIO_TIMING	= 0x40,
	ATIIXP_IDE_MWDMA_TIMING	= 0x44,
	ATIIXP_IDE_PIO_CONTROL	= 0x48,
	ATIIXP_IDE_PIO_MODE	= 0x4a,
	ATIIXP_IDE_UDMA_CONTROL	= 0x54,
	ATIIXP_IDE_UDMA_MODE 	= 0x56
};

static int atiixp_cable_detect(struct ata_port *ap)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u8 udma;

	/* Hack from drivers/ide/pci. Really we want to know how to do the
	   raw detection not play follow the bios mode guess */
	pci_read_config_byte(pdev, ATIIXP_IDE_UDMA_MODE + ap->port_no, &udma);
	if ((udma & 0x07) >= 0x04 || (udma & 0x70) >= 0x40)
		return  ATA_CBL_PATA80;
	return ATA_CBL_PATA40;
}


static void atiixp_set_pio_timing(struct ata_port *ap, struct ata_device *adev, int pio)
{
	static u8 pio_timings[5] = { 0x5D, 0x47, 0x34, 0x22, 0x20 };

	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int dn = 2 * ap->port_no + adev->devno;

	/* Check this is correct - the order is odd in both drivers */
	int timing_shift = (16 * ap->port_no) + 8 * (adev->devno ^ 1);
	u16 pio_mode_data, pio_timing_data;

	pci_read_config_word(pdev, ATIIXP_IDE_PIO_MODE, &pio_mode_data);
	pio_mode_data &= ~(0x7 << (4 * dn));
	pio_mode_data |= pio << (4 * dn);
	pci_write_config_word(pdev, ATIIXP_IDE_PIO_MODE, pio_mode_data);

	pci_read_config_word(pdev, ATIIXP_IDE_PIO_TIMING, &pio_timing_data);
	pio_timing_data &= ~(0xFF << timing_shift);
	pio_timing_data |= (pio_timings[pio] << timing_shift);
	pci_write_config_word(pdev, ATIIXP_IDE_PIO_TIMING, pio_timing_data);
}


static void atiixp_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	atiixp_set_pio_timing(ap, adev, adev->pio_mode - XFER_PIO_0);
}


static void atiixp_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	static u8 mwdma_timings[5] = { 0x77, 0x21, 0x20 };

	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int dma = adev->dma_mode;
	int dn = 2 * ap->port_no + adev->devno;
	int wanted_pio;

	if (adev->dma_mode >= XFER_UDMA_0) {
		u16 udma_mode_data;

		dma -= XFER_UDMA_0;

		pci_read_config_word(pdev, ATIIXP_IDE_UDMA_MODE, &udma_mode_data);
		udma_mode_data &= ~(0x7 << (4 * dn));
		udma_mode_data |= dma << (4 * dn);
		pci_write_config_word(pdev, ATIIXP_IDE_UDMA_MODE, udma_mode_data);
	} else {
		u16 mwdma_timing_data;
		/* Check this is correct - the order is odd in both drivers */
		int timing_shift = (16 * ap->port_no) + 8 * (adev->devno ^ 1);

		dma -= XFER_MW_DMA_0;

		pci_read_config_word(pdev, ATIIXP_IDE_MWDMA_TIMING, &mwdma_timing_data);
		mwdma_timing_data &= ~(0xFF << timing_shift);
		mwdma_timing_data |= (mwdma_timings[dma] << timing_shift);
		pci_write_config_word(pdev, ATIIXP_IDE_MWDMA_TIMING, mwdma_timing_data);
	}
	/*
	 *	We must now look at the PIO mode situation. We may need to
	 *	adjust the PIO mode to keep the timings acceptable
	 */
	 if (adev->dma_mode >= XFER_MW_DMA_2)
	 	wanted_pio = 4;
	else if (adev->dma_mode == XFER_MW_DMA_1)
		wanted_pio = 3;
	else if (adev->dma_mode == XFER_MW_DMA_0)
		wanted_pio = 0;
	else BUG();

	if (adev->pio_mode != wanted_pio)
		atiixp_set_pio_timing(ap, adev, wanted_pio);
}


static void atiixp_bmdma_start(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ata_device *adev = qc->dev;

	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int dn = (2 * ap->port_no) + adev->devno;
	u16 tmp16;

	pci_read_config_word(pdev, ATIIXP_IDE_UDMA_CONTROL, &tmp16);
	if (ata_using_udma(adev))
		tmp16 |= (1 << dn);
	else
		tmp16 &= ~(1 << dn);
	pci_write_config_word(pdev, ATIIXP_IDE_UDMA_CONTROL, tmp16);
	ata_bmdma_start(qc);
}


static void atiixp_bmdma_stop(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int dn = (2 * ap->port_no) + qc->dev->devno;
	u16 tmp16;

	pci_read_config_word(pdev, ATIIXP_IDE_UDMA_CONTROL, &tmp16);
	tmp16 &= ~(1 << dn);
	pci_write_config_word(pdev, ATIIXP_IDE_UDMA_CONTROL, tmp16);
	ata_bmdma_stop(qc);
}

static struct scsi_host_template atiixp_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
	.sg_tablesize		= LIBATA_DUMB_MAX_PRD,
};

static struct ata_port_operations atiixp_port_ops = {
	.inherits	= &ata_bmdma_port_ops,

	.qc_prep 	= ata_sff_dumb_qc_prep,
	.bmdma_start 	= atiixp_bmdma_start,
	.bmdma_stop	= atiixp_bmdma_stop,

	.cable_detect	= atiixp_cable_detect,
	.set_piomode	= atiixp_set_piomode,
	.set_dmamode	= atiixp_set_dmamode,
};

static int atiixp_init_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	static const struct ata_port_info info = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = 0x1f,
		.mwdma_mask = 0x06,	/* No MWDMA0 support */
		.udma_mask = 0x3F,
		.port_ops = &atiixp_port_ops
	};
	static const struct pci_bits atiixp_enable_bits[] = {
		{ 0x48, 1, 0x01, 0x00 },
		{ 0x48, 1, 0x08, 0x00 }
	};
	const struct ata_port_info *ppi[] = { &info, &info };
	int i;

	for (i = 0; i < 2; i++)
		if (!pci_test_config_bits(pdev, &atiixp_enable_bits[i]))
			ppi[i] = &ata_dummy_port_info;

	return ata_pci_sff_init_one(pdev, ppi, &atiixp_sht, NULL);
}

static const struct pci_device_id atiixp[] = {
	{ PCI_VDEVICE(ATI, PCI_DEVICE_ID_ATI_IXP200_IDE), },
	{ PCI_VDEVICE(ATI, PCI_DEVICE_ID_ATI_IXP300_IDE), },
	{ PCI_VDEVICE(ATI, PCI_DEVICE_ID_ATI_IXP400_IDE), },
	{ PCI_VDEVICE(ATI, PCI_DEVICE_ID_ATI_IXP600_IDE), },
	{ PCI_VDEVICE(ATI, PCI_DEVICE_ID_ATI_IXP700_IDE), },

	{ },
};

static struct pci_driver atiixp_pci_driver = {
	.name 		= DRV_NAME,
	.id_table	= atiixp,
	.probe 		= atiixp_init_one,
	.remove		= ata_pci_remove_one,
#ifdef CONFIG_PM
	.resume		= ata_pci_device_resume,
	.suspend	= ata_pci_device_suspend,
#endif
};

static int __init atiixp_init(void)
{
	return pci_register_driver(&atiixp_pci_driver);
}


static void __exit atiixp_exit(void)
{
	pci_unregister_driver(&atiixp_pci_driver);
}

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("low-level driver for ATI IXP200/300/400");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, atiixp);
MODULE_VERSION(DRV_VERSION);

module_init(atiixp_init);
module_exit(atiixp_exit);