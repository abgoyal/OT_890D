

#include <linux/list.h>
#include <linux/kvm_host.h>
#include <linux/pci.h>
#include <linux/dmar.h>
#include <linux/iommu.h>
#include <linux/intel-iommu.h>

static int kvm_iommu_unmap_memslots(struct kvm *kvm);
static void kvm_iommu_put_pages(struct kvm *kvm,
				gfn_t base_gfn, unsigned long npages);

int kvm_iommu_map_pages(struct kvm *kvm,
			gfn_t base_gfn, unsigned long npages)
{
	gfn_t gfn = base_gfn;
	pfn_t pfn;
	int i, r = 0;
	struct iommu_domain *domain = kvm->arch.iommu_domain;

	/* check if iommu exists and in use */
	if (!domain)
		return 0;

	for (i = 0; i < npages; i++) {
		/* check if already mapped */
		if (iommu_iova_to_phys(domain, gfn_to_gpa(gfn)))
			continue;

		pfn = gfn_to_pfn(kvm, gfn);
		r = iommu_map_range(domain,
				    gfn_to_gpa(gfn),
				    pfn_to_hpa(pfn),
				    PAGE_SIZE,
				    IOMMU_READ | IOMMU_WRITE);
		if (r) {
			printk(KERN_ERR "kvm_iommu_map_address:"
			       "iommu failed to map pfn=%lx\n", pfn);
			goto unmap_pages;
		}
		gfn++;
	}
	return 0;

unmap_pages:
	kvm_iommu_put_pages(kvm, base_gfn, i);
	return r;
}

static int kvm_iommu_map_memslots(struct kvm *kvm)
{
	int i, r = 0;

	for (i = 0; i < kvm->nmemslots; i++) {
		r = kvm_iommu_map_pages(kvm, kvm->memslots[i].base_gfn,
					kvm->memslots[i].npages);
		if (r)
			break;
	}

	return r;
}

int kvm_assign_device(struct kvm *kvm,
		      struct kvm_assigned_dev_kernel *assigned_dev)
{
	struct pci_dev *pdev = NULL;
	struct iommu_domain *domain = kvm->arch.iommu_domain;
	int r;

	/* check if iommu exists and in use */
	if (!domain)
		return 0;

	pdev = assigned_dev->dev;
	if (pdev == NULL)
		return -ENODEV;

	r = iommu_attach_device(domain, &pdev->dev);
	if (r) {
		printk(KERN_ERR "assign device %x:%x.%x failed",
			pdev->bus->number,
			PCI_SLOT(pdev->devfn),
			PCI_FUNC(pdev->devfn));
		return r;
	}

	printk(KERN_DEBUG "assign device: host bdf = %x:%x:%x\n",
		assigned_dev->host_busnr,
		PCI_SLOT(assigned_dev->host_devfn),
		PCI_FUNC(assigned_dev->host_devfn));

	return 0;
}

int kvm_deassign_device(struct kvm *kvm,
			struct kvm_assigned_dev_kernel *assigned_dev)
{
	struct iommu_domain *domain = kvm->arch.iommu_domain;
	struct pci_dev *pdev = NULL;

	/* check if iommu exists and in use */
	if (!domain)
		return 0;

	pdev = assigned_dev->dev;
	if (pdev == NULL)
		return -ENODEV;

	iommu_detach_device(domain, &pdev->dev);

	printk(KERN_DEBUG "deassign device: host bdf = %x:%x:%x\n",
		assigned_dev->host_busnr,
		PCI_SLOT(assigned_dev->host_devfn),
		PCI_FUNC(assigned_dev->host_devfn));

	return 0;
}

int kvm_iommu_map_guest(struct kvm *kvm)
{
	int r;

	if (!iommu_found()) {
		printk(KERN_ERR "%s: iommu not found\n", __func__);
		return -ENODEV;
	}

	kvm->arch.iommu_domain = iommu_domain_alloc();
	if (!kvm->arch.iommu_domain)
		return -ENOMEM;

	r = kvm_iommu_map_memslots(kvm);
	if (r)
		goto out_unmap;

	return 0;

out_unmap:
	kvm_iommu_unmap_memslots(kvm);
	return r;
}

static void kvm_iommu_put_pages(struct kvm *kvm,
				gfn_t base_gfn, unsigned long npages)
{
	gfn_t gfn = base_gfn;
	pfn_t pfn;
	struct iommu_domain *domain = kvm->arch.iommu_domain;
	unsigned long i;
	u64 phys;

	/* check if iommu exists and in use */
	if (!domain)
		return;

	for (i = 0; i < npages; i++) {
		phys = iommu_iova_to_phys(domain, gfn_to_gpa(gfn));
		pfn = phys >> PAGE_SHIFT;
		kvm_release_pfn_clean(pfn);
		gfn++;
	}

	iommu_unmap_range(domain, gfn_to_gpa(base_gfn), PAGE_SIZE * npages);
}

static int kvm_iommu_unmap_memslots(struct kvm *kvm)
{
	int i;

	for (i = 0; i < kvm->nmemslots; i++) {
		kvm_iommu_put_pages(kvm, kvm->memslots[i].base_gfn,
				    kvm->memslots[i].npages);
	}

	return 0;
}

int kvm_iommu_unmap_guest(struct kvm *kvm)
{
	struct iommu_domain *domain = kvm->arch.iommu_domain;

	/* check if iommu exists and in use */
	if (!domain)
		return 0;

	kvm_iommu_unmap_memslots(kvm);
	iommu_domain_free(domain);
	return 0;
}
