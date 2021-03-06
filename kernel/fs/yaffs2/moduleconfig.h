

#ifndef __YAFFS_CONFIG_H__
#define __YAFFS_CONFIG_H__

#ifdef YAFFS_OUT_OF_TREE

/* DO NOT UNSET THESE THREE. YAFFS2 will not compile if you do. */
#define CONFIG_YAFFS_FS
#define CONFIG_YAFFS_YAFFS1
#define CONFIG_YAFFS_YAFFS2

/* These options are independent of each other.  Select those that matter. */

/* Default: Not selected */
/* Meaning: Yaffs does its own ECC, rather than using MTD ECC */
/* #define CONFIG_YAFFS_DOES_ECC */

/* Default: Not selected */
/* Meaning: ECC byte order is 'wrong'.  Only meaningful if */
/*          CONFIG_YAFFS_DOES_ECC is set */
/* #define CONFIG_YAFFS_ECC_WRONG_ORDER */

/* Default: Selected */
/* Meaning: Disables testing whether chunks are erased before writing to them*/
#define CONFIG_YAFFS_DISABLE_CHUNK_ERASED_CHECK

/* Default: Selected */
/* Meaning: Cache short names, taking more RAM, but faster look-ups */
#define CONFIG_YAFFS_SHORT_NAMES_IN_RAM

/* Default: 10 */
/* Meaning: set the count of blocks to reserve for checkpointing */
#define CONFIG_YAFFS_CHECKPOINT_RESERVED_BLOCKS 10

/* Default: Not selected */
/* Meaning: Use older-style on-NAND data format with pageStatus byte */
/* #define CONFIG_YAFFS_9BYTE_TAGS */

#endif /* YAFFS_OUT_OF_TREE */

#endif /* __YAFFS_CONFIG_H__ */
