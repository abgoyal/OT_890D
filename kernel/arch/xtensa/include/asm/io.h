

#ifndef _XTENSA_IO_H
#define _XTENSA_IO_H

#ifdef __KERNEL__
#include <asm/byteorder.h>
#include <asm/page.h>
#include <linux/kernel.h>

#include <linux/types.h>

#define XCHAL_KIO_CACHED_VADDR	0xe0000000
#define XCHAL_KIO_BYPASS_VADDR	0xf0000000
#define XCHAL_KIO_PADDR		0xf0000000
#define XCHAL_KIO_SIZE		0x10000000

#define IOADDR(x)		(XCHAL_KIO_BYPASS_VADDR + (x))


static inline unsigned short _swapw (unsigned short v)
{
	return (v << 8) | (v >> 8);
}

static inline unsigned int _swapl (unsigned int v)
{
	return (v << 24) | ((v & 0xff00) << 8) | ((v >> 8) & 0xff00) | (v >> 24);
}


static inline unsigned long virt_to_phys(volatile void * address)
{
	return __pa(address);
}

static inline void * phys_to_virt(unsigned long address)
{
	return __va(address);
}


#define virt_to_bus(x)	virt_to_phys(x)
#define bus_to_virt(x)	phys_to_virt(x)


static inline void *ioremap(unsigned long offset, unsigned long size)
{
	if (offset >= XCHAL_KIO_PADDR
	    && offset < XCHAL_KIO_PADDR + XCHAL_KIO_SIZE)
		return (void*)(offset-XCHAL_KIO_PADDR+XCHAL_KIO_BYPASS_VADDR);

	else
		BUG();
}

static inline void *ioremap_nocache(unsigned long offset, unsigned long size)
{
	if (offset >= XCHAL_KIO_PADDR
	    && offset < XCHAL_KIO_PADDR + XCHAL_KIO_SIZE)
		return (void*)(offset-XCHAL_KIO_PADDR+XCHAL_KIO_CACHED_VADDR);
	else
		BUG();
}

static inline void iounmap(void *addr)
{
}


#define readb(addr) \
	({ unsigned char __v = (*(volatile unsigned char *)(addr)); __v; })
#define readw(addr) \
	({ unsigned short __v = (*(volatile unsigned short *)(addr)); __v; })
#define readl(addr) \
	({ unsigned int __v = (*(volatile unsigned int *)(addr)); __v; })
#define writeb(b, addr) (void)((*(volatile unsigned char *)(addr)) = (b))
#define writew(b, addr) (void)((*(volatile unsigned short *)(addr)) = (b))
#define writel(b, addr) (void)((*(volatile unsigned int *)(addr)) = (b))

static inline __u8 __raw_readb(const volatile void __iomem *addr)
{
          return *(__force volatile __u8 *)(addr);
}
static inline __u16 __raw_readw(const volatile void __iomem *addr)
{
          return *(__force volatile __u16 *)(addr);
}
static inline __u32 __raw_readl(const volatile void __iomem *addr)
{
          return *(__force volatile __u32 *)(addr);
}
static inline void __raw_writeb(__u8 b, volatile void __iomem *addr)
{
          *(__force volatile __u8 *)(addr) = b;
}
static inline void __raw_writew(__u16 b, volatile void __iomem *addr)
{
          *(__force volatile __u16 *)(addr) = b;
}
static inline void __raw_writel(__u32 b, volatile void __iomem *addr)
{
          *(__force volatile __u32 *)(addr) = b;
}


#define inb(port)		readb((u8 *)((port)))
#define outb(val, port)		writeb((val),(u8 *)((unsigned long)(port)))
#define inw(port)		readw((u16 *)((port)))
#define outw(val, port)		writew((val),(u16 *)((unsigned long)(port)))
#define inl(port)		readl((u32 *)((port)))
#define outl(val, port)		writel((val),(u32 *)((unsigned long)(port)))

#define inb_p(port)		inb((port))
#define outb_p(val, port)	outb((val), (port))
#define inw_p(port)		inw((port))
#define outw_p(val, port)	outw((val), (port))
#define inl_p(port)		inl((port))
#define outl_p(val, port)	outl((val), (port))

extern void insb (unsigned long port, void *dst, unsigned long count);
extern void insw (unsigned long port, void *dst, unsigned long count);
extern void insl (unsigned long port, void *dst, unsigned long count);
extern void outsb (unsigned long port, const void *src, unsigned long count);
extern void outsw (unsigned long port, const void *src, unsigned long count);
extern void outsl (unsigned long port, const void *src, unsigned long count);

#define IO_SPACE_LIMIT ~0

#define memset_io(a,b,c)       memset((void *)(a),(b),(c))
#define memcpy_fromio(a,b,c)   memcpy((a),(void *)(b),(c))
#define memcpy_toio(a,b,c)      memcpy((void *)(a),(b),(c))

/* At this point the Xtensa doesn't provide byte swap instructions */

#ifdef __XTENSA_EB__
# define in_8(addr) (*(u8*)(addr))
# define in_le16(addr) _swapw(*(u16*)(addr))
# define in_le32(addr) _swapl(*(u32*)(addr))
# define out_8(b, addr) *(u8*)(addr) = (b)
# define out_le16(b, addr) *(u16*)(addr) = _swapw(b)
# define out_le32(b, addr) *(u32*)(addr) = _swapl(b)
#elif defined(__XTENSA_EL__)
# define in_8(addr)  (*(u8*)(addr))
# define in_le16(addr) (*(u16*)(addr))
# define in_le32(addr) (*(u32*)(addr))
# define out_8(b, addr) *(u8*)(addr) = (b)
# define out_le16(b, addr) *(u16*)(addr) = (b)
# define out_le32(b, addr) *(u32*)(addr) = (b)
#else
# error processor byte order undefined!
#endif


#define xlate_dev_mem_ptr(p)    __va(p)

#define xlate_dev_kmem_ptr(p)   p


#endif	/* __KERNEL__ */

#endif	/* _XTENSA_IO_H */
