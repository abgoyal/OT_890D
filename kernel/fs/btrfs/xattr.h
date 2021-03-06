

#ifndef __XATTR__
#define __XATTR__

#include <linux/xattr.h>

extern struct xattr_handler btrfs_xattr_acl_access_handler;
extern struct xattr_handler btrfs_xattr_acl_default_handler;
extern struct xattr_handler *btrfs_xattr_handlers[];

extern ssize_t __btrfs_getxattr(struct inode *inode, const char *name,
		void *buffer, size_t size);
extern int __btrfs_setxattr(struct inode *inode, const char *name,
		const void *value, size_t size, int flags);

extern ssize_t btrfs_getxattr(struct dentry *dentry, const char *name,
		void *buffer, size_t size);
extern int btrfs_setxattr(struct dentry *dentry, const char *name,
		const void *value, size_t size, int flags);
extern int btrfs_removexattr(struct dentry *dentry, const char *name);

extern int btrfs_xattr_security_init(struct inode *inode, struct inode *dir);

#endif /* __XATTR__ */
