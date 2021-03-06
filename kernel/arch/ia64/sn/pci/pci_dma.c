

#include <linux/module.h>
#include <linux/dma-attrs.h>
#include <asm/dma.h>
#include <asm/sn/intr.h>
#include <asm/sn/pcibus_provider_defs.h>
#include <asm/sn/pcidev.h>
#include <asm/sn/sn_sal.h>

#define SG_ENT_VIRT_ADDRESS(sg)	(sg_virt((sg)))
#define SG_ENT_PHYS_ADDRESS(SG)	virt_to_phys(SG_ENT_VIRT_ADDRESS(SG))

int sn_dma_supported(struct device *dev, u64 mask)
{
	BUG_ON(dev->bus != &pci_bus_type);

	if (mask < 0x7fffffff)
		return 0;
	return 1;
}
EXPORT_SYMBOL(sn_dma_supported);

int sn_dma_set_mask(struct device *dev, u64 dma_mask)
{
	BUG_ON(dev->bus != &pci_bus_type);

	if (!sn_dma_supported(dev, dma_mask))
		return 0;

	*dev->dma_mask = dma_mask;
	return 1;
}
EXPORT_SYMBOL(sn_dma_set_mask);

void *sn_dma_alloc_coherent(struct device *dev, size_t size,
			    dma_addr_t * dma_handle, gfp_t flags)
{
	void *cpuaddr;
	unsigned long phys_addr;
	int node;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct sn_pcibus_provider *provider = SN_PCIDEV_BUSPROVIDER(pdev);

	BUG_ON(dev->bus != &pci_bus_type);

	/*
	 * Allocate the memory.
	 */
	node = pcibus_to_node(pdev->bus);
	if (likely(node >=0)) {
		struct page *p = alloc_pages_node(node, flags, get_order(size));

		if (likely(p))
			cpuaddr = page_address(p);
		else
			return NULL;
	} else
		cpuaddr = (void *)__get_free_pages(flags, get_order(size));

	if (unlikely(!cpuaddr))
		return NULL;

	memset(cpuaddr, 0x0, size);

	/* physical addr. of the memory we just got */
	phys_addr = __pa(cpuaddr);

	/*
	 * 64 bit address translations should never fail.
	 * 32 bit translations can fail if there are insufficient mapping
	 * resources.
	 */

	*dma_handle = provider->dma_map_consistent(pdev, phys_addr, size,
						   SN_DMA_ADDR_PHYS);
	if (!*dma_handle) {
		printk(KERN_ERR "%s: out of ATEs\n", __func__);
		free_pages((unsigned long)cpuaddr, get_order(size));
		return NULL;
	}

	return cpuaddr;
}
EXPORT_SYMBOL(sn_dma_alloc_coherent);

void sn_dma_free_coherent(struct device *dev, size_t size, void *cpu_addr,
			  dma_addr_t dma_handle)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct sn_pcibus_provider *provider = SN_PCIDEV_BUSPROVIDER(pdev);

	BUG_ON(dev->bus != &pci_bus_type);

	provider->dma_unmap(pdev, dma_handle, 0);
	free_pages((unsigned long)cpu_addr, get_order(size));
}
EXPORT_SYMBOL(sn_dma_free_coherent);

dma_addr_t sn_dma_map_single_attrs(struct device *dev, void *cpu_addr,
				   size_t size, int direction,
				   struct dma_attrs *attrs)
{
	dma_addr_t dma_addr;
	unsigned long phys_addr;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct sn_pcibus_provider *provider = SN_PCIDEV_BUSPROVIDER(pdev);
	int dmabarr;

	dmabarr = dma_get_attr(DMA_ATTR_WRITE_BARRIER, attrs);

	BUG_ON(dev->bus != &pci_bus_type);

	phys_addr = __pa(cpu_addr);
	if (dmabarr)
		dma_addr = provider->dma_map_consistent(pdev, phys_addr,
							size, SN_DMA_ADDR_PHYS);
	else
		dma_addr = provider->dma_map(pdev, phys_addr, size,
					     SN_DMA_ADDR_PHYS);

	if (!dma_addr) {
		printk(KERN_ERR "%s: out of ATEs\n", __func__);
		return 0;
	}
	return dma_addr;
}
EXPORT_SYMBOL(sn_dma_map_single_attrs);

void sn_dma_unmap_single_attrs(struct device *dev, dma_addr_t dma_addr,
			       size_t size, int direction,
			       struct dma_attrs *attrs)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct sn_pcibus_provider *provider = SN_PCIDEV_BUSPROVIDER(pdev);

	BUG_ON(dev->bus != &pci_bus_type);

	provider->dma_unmap(pdev, dma_addr, direction);
}
EXPORT_SYMBOL(sn_dma_unmap_single_attrs);

void sn_dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sgl,
			   int nhwentries, int direction,
			   struct dma_attrs *attrs)
{
	int i;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct sn_pcibus_provider *provider = SN_PCIDEV_BUSPROVIDER(pdev);
	struct scatterlist *sg;

	BUG_ON(dev->bus != &pci_bus_type);

	for_each_sg(sgl, sg, nhwentries, i) {
		provider->dma_unmap(pdev, sg->dma_address, direction);
		sg->dma_address = (dma_addr_t) NULL;
		sg->dma_length = 0;
	}
}
EXPORT_SYMBOL(sn_dma_unmap_sg_attrs);

int sn_dma_map_sg_attrs(struct device *dev, struct scatterlist *sgl,
			int nhwentries, int direction, struct dma_attrs *attrs)
{
	unsigned long phys_addr;
	struct scatterlist *saved_sg = sgl, *sg;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct sn_pcibus_provider *provider = SN_PCIDEV_BUSPROVIDER(pdev);
	int i;
	int dmabarr;

	dmabarr = dma_get_attr(DMA_ATTR_WRITE_BARRIER, attrs);

	BUG_ON(dev->bus != &pci_bus_type);

	/*
	 * Setup a DMA address for each entry in the scatterlist.
	 */
	for_each_sg(sgl, sg, nhwentries, i) {
		dma_addr_t dma_addr;
		phys_addr = SG_ENT_PHYS_ADDRESS(sg);
		if (dmabarr)
			dma_addr = provider->dma_map_consistent(pdev,
								phys_addr,
								sg->length,
								SN_DMA_ADDR_PHYS);
		else
			dma_addr = provider->dma_map(pdev, phys_addr,
						     sg->length,
						     SN_DMA_ADDR_PHYS);

		sg->dma_address = dma_addr;
		if (!sg->dma_address) {
			printk(KERN_ERR "%s: out of ATEs\n", __func__);

			/*
			 * Free any successfully allocated entries.
			 */
			if (i > 0)
				sn_dma_unmap_sg_attrs(dev, saved_sg, i,
						      direction, attrs);
			return 0;
		}

		sg->dma_length = sg->length;
	}

	return nhwentries;
}
EXPORT_SYMBOL(sn_dma_map_sg_attrs);

void sn_dma_sync_single_for_cpu(struct device *dev, dma_addr_t dma_handle,
				size_t size, int direction)
{
	BUG_ON(dev->bus != &pci_bus_type);
}
EXPORT_SYMBOL(sn_dma_sync_single_for_cpu);

void sn_dma_sync_single_for_device(struct device *dev, dma_addr_t dma_handle,
				   size_t size, int direction)
{
	BUG_ON(dev->bus != &pci_bus_type);
}
EXPORT_SYMBOL(sn_dma_sync_single_for_device);

void sn_dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg,
			    int nelems, int direction)
{
	BUG_ON(dev->bus != &pci_bus_type);
}
EXPORT_SYMBOL(sn_dma_sync_sg_for_cpu);

void sn_dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg,
			       int nelems, int direction)
{
	BUG_ON(dev->bus != &pci_bus_type);
}
EXPORT_SYMBOL(sn_dma_sync_sg_for_device);

int sn_dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return 0;
}
EXPORT_SYMBOL(sn_dma_mapping_error);

u64 sn_dma_get_required_mask(struct device *dev)
{
	return DMA_64BIT_MASK;
}
EXPORT_SYMBOL_GPL(sn_dma_get_required_mask);

char *sn_pci_get_legacy_mem(struct pci_bus *bus)
{
	if (!SN_PCIBUS_BUSSOFT(bus))
		return ERR_PTR(-ENODEV);

	return (char *)(SN_PCIBUS_BUSSOFT(bus)->bs_legacy_mem | __IA64_UNCACHED_OFFSET);
}

int sn_pci_legacy_read(struct pci_bus *bus, u16 port, u32 *val, u8 size)
{
	unsigned long addr;
	int ret;
	struct ia64_sal_retval isrv;

	/*
	 * First, try the SN_SAL_IOIF_PCI_SAFE SAL call which can work
	 * around hw issues at the pci bus level.  SGI proms older than
	 * 4.10 don't implement this.
	 */

	SAL_CALL(isrv, SN_SAL_IOIF_PCI_SAFE,
		 pci_domain_nr(bus), bus->number,
		 0, /* io */
		 0, /* read */
		 port, size, __pa(val));

	if (isrv.status == 0)
		return size;

	/*
	 * If the above failed, retry using the SAL_PROBE call which should
	 * be present in all proms (but which cannot work round PCI chipset
	 * bugs).  This code is retained for compatibility with old
	 * pre-4.10 proms, and should be removed at some point in the future.
	 */

	if (!SN_PCIBUS_BUSSOFT(bus))
		return -ENODEV;

	addr = SN_PCIBUS_BUSSOFT(bus)->bs_legacy_io | __IA64_UNCACHED_OFFSET;
	addr += port;

	ret = ia64_sn_probe_mem(addr, (long)size, (void *)val);

	if (ret == 2)
		return -EINVAL;

	if (ret == 1)
		*val = -1;

	return size;
}

int sn_pci_legacy_write(struct pci_bus *bus, u16 port, u32 val, u8 size)
{
	int ret = size;
	unsigned long paddr;
	unsigned long *addr;
	struct ia64_sal_retval isrv;

	/*
	 * First, try the SN_SAL_IOIF_PCI_SAFE SAL call which can work
	 * around hw issues at the pci bus level.  SGI proms older than
	 * 4.10 don't implement this.
	 */

	SAL_CALL(isrv, SN_SAL_IOIF_PCI_SAFE,
		 pci_domain_nr(bus), bus->number,
		 0, /* io */
		 1, /* write */
		 port, size, __pa(&val));

	if (isrv.status == 0)
		return size;

	/*
	 * If the above failed, retry using the SAL_PROBE call which should
	 * be present in all proms (but which cannot work round PCI chipset
	 * bugs).  This code is retained for compatibility with old
	 * pre-4.10 proms, and should be removed at some point in the future.
	 */

	if (!SN_PCIBUS_BUSSOFT(bus)) {
		ret = -ENODEV;
		goto out;
	}

	/* Put the phys addr in uncached space */
	paddr = SN_PCIBUS_BUSSOFT(bus)->bs_legacy_io | __IA64_UNCACHED_OFFSET;
	paddr += port;
	addr = (unsigned long *)paddr;

	switch (size) {
	case 1:
		*(volatile u8 *)(addr) = (u8)(val);
		break;
	case 2:
		*(volatile u16 *)(addr) = (u16)(val);
		break;
	case 4:
		*(volatile u32 *)(addr) = (u32)(val);
		break;
	default:
		ret = -EINVAL;
		break;
	}
 out:
	return ret;
}
