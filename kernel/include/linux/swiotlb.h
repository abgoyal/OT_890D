
#ifndef __LINUX_SWIOTLB_H
#define __LINUX_SWIOTLB_H

#include <linux/types.h>

struct device;
struct dma_attrs;
struct scatterlist;

#define IO_TLB_SEGSIZE	128


#define IO_TLB_SHIFT 11

extern void
swiotlb_init(void);

extern void *swiotlb_alloc_boot(size_t bytes, unsigned long nslabs);
extern void *swiotlb_alloc(unsigned order, unsigned long nslabs);

extern dma_addr_t swiotlb_phys_to_bus(struct device *hwdev,
				      phys_addr_t address);
extern phys_addr_t swiotlb_bus_to_phys(dma_addr_t address);

extern int swiotlb_arch_range_needs_mapping(void *ptr, size_t size);

extern void
*swiotlb_alloc_coherent(struct device *hwdev, size_t size,
			dma_addr_t *dma_handle, gfp_t flags);

extern void
swiotlb_free_coherent(struct device *hwdev, size_t size,
		      void *vaddr, dma_addr_t dma_handle);

extern dma_addr_t
swiotlb_map_single(struct device *hwdev, void *ptr, size_t size, int dir);

extern void
swiotlb_unmap_single(struct device *hwdev, dma_addr_t dev_addr,
		     size_t size, int dir);

extern dma_addr_t
swiotlb_map_single_attrs(struct device *hwdev, void *ptr, size_t size,
			 int dir, struct dma_attrs *attrs);

extern void
swiotlb_unmap_single_attrs(struct device *hwdev, dma_addr_t dev_addr,
			   size_t size, int dir, struct dma_attrs *attrs);

extern int
swiotlb_map_sg(struct device *hwdev, struct scatterlist *sg, int nents,
	       int direction);

extern void
swiotlb_unmap_sg(struct device *hwdev, struct scatterlist *sg, int nents,
		 int direction);

extern int
swiotlb_map_sg_attrs(struct device *hwdev, struct scatterlist *sgl, int nelems,
		     int dir, struct dma_attrs *attrs);

extern void
swiotlb_unmap_sg_attrs(struct device *hwdev, struct scatterlist *sgl,
		       int nelems, int dir, struct dma_attrs *attrs);

extern void
swiotlb_sync_single_for_cpu(struct device *hwdev, dma_addr_t dev_addr,
			    size_t size, int dir);

extern void
swiotlb_sync_sg_for_cpu(struct device *hwdev, struct scatterlist *sg,
			int nelems, int dir);

extern void
swiotlb_sync_single_for_device(struct device *hwdev, dma_addr_t dev_addr,
			       size_t size, int dir);

extern void
swiotlb_sync_sg_for_device(struct device *hwdev, struct scatterlist *sg,
			   int nelems, int dir);

extern void
swiotlb_sync_single_range_for_cpu(struct device *hwdev, dma_addr_t dev_addr,
				  unsigned long offset, size_t size, int dir);

extern void
swiotlb_sync_single_range_for_device(struct device *hwdev, dma_addr_t dev_addr,
				     unsigned long offset, size_t size,
				     int dir);

extern int
swiotlb_dma_mapping_error(struct device *hwdev, dma_addr_t dma_addr);

extern int
swiotlb_dma_supported(struct device *hwdev, u64 mask);

#endif /* __LINUX_SWIOTLB_H */
