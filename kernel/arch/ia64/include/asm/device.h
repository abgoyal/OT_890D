
#ifndef _ASM_IA64_DEVICE_H
#define _ASM_IA64_DEVICE_H

struct dev_archdata {
#ifdef CONFIG_ACPI
	void	*acpi_handle;
#endif
#ifdef CONFIG_DMAR
	void *iommu; /* hook for IOMMU specific extension */
#endif
};

#endif /* _ASM_IA64_DEVICE_H */
