
#ifndef _CRYPTO_ARCH_S390_SHA_H
#define _CRYPTO_ARCH_S390_SHA_H

#include <linux/crypto.h>
#include <crypto/sha.h>

/* must be big enough for the largest SHA variant */
#define SHA_MAX_STATE_SIZE	16
#define SHA_MAX_BLOCK_SIZE      SHA512_BLOCK_SIZE

struct s390_sha_ctx {
	u64 count;              /* message length in bytes */
	u32 state[SHA_MAX_STATE_SIZE];
	u8 buf[2 * SHA_MAX_BLOCK_SIZE];
	int func;		/* KIMD function to use */
};

void s390_sha_update(struct crypto_tfm *tfm, const u8 *data, unsigned int len);
void s390_sha_final(struct crypto_tfm *tfm, u8 *out);

#endif
