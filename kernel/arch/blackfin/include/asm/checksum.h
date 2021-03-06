
#ifndef _BFIN_CHECKSUM_H
#define _BFIN_CHECKSUM_H

__wsum csum_partial(const void *buff, int len, __wsum sum);


__wsum csum_partial_copy(const void *src, void *dst,
			       int len, __wsum sum);


extern __wsum csum_partial_copy_from_user(const void __user *src, void *dst,
					  int len, __wsum sum, int *csum_err);

#define csum_partial_copy_nocheck(src, dst, len, sum)	\
	csum_partial_copy((src), (dst), (len), (sum))

__sum16 ip_fast_csum(unsigned char *iph, unsigned int ihl);


static inline __sum16 csum_fold(__wsum sum)
{
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	return ((~(sum << 16)) >> 16);
}


static inline __wsum
csum_tcpudp_nofold(__be32 saddr, __be32 daddr, unsigned short len,
		   unsigned short proto, __wsum sum)
{
	unsigned int carry;

	__asm__ ("%0 = %0 + %2;\n\t"
		"CC = AC0;\n\t"
		"%1 = CC;\n\t"
		"%0 = %0 + %1;\n\t"
		"%0 = %0 + %3;\n\t"
		"CC = AC0;\n\t"
		"%1 = CC;\n\t"
		"%0 = %0 + %1;\n\t"
		"%0 = %0 + %4;\n\t"
		"CC = AC0;\n\t"
		"%1 = CC;\n\t"
		"%0 = %0 + %1;\n\t"
		: "=d" (sum), "=&d" (carry)
		: "d" (daddr), "d" (saddr), "d" ((len + proto) << 8), "0"(sum)
		: "CC");

	return (sum);
}

static inline __sum16
csum_tcpudp_magic(__be32 saddr, __be32 daddr, unsigned short len,
		  unsigned short proto, __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr, daddr, len, proto, sum));
}


extern __sum16 ip_compute_csum(const void *buff, int len);

#endif				/* _BFIN_CHECKSUM_H */
