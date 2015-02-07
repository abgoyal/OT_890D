
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_bit.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir2.h"
#include "xfs_alloc.h"
#include "xfs_dmapi.h"
#include "xfs_quota.h"
#include "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_dir2_sf.h"
#include "xfs_attr_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_bmap.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_rtalloc.h"
#include "xfs_error.h"
#include "xfs_itable.h"
#include "xfs_rw.h"
#include "xfs_acl.h"
#include "xfs_attr.h"
#include "xfs_buf_item.h"
#include "xfs_utils.h"
#include "xfs_vnodeops.h"

#include <linux/capability.h>
#include <linux/xattr.h>
#include <linux/namei.h>
#include <linux/security.h>
#include <linux/falloc.h>
#include <linux/fiemap.h>

void
xfs_synchronize_atime(
	xfs_inode_t	*ip)
{
	struct inode	*inode = VFS_I(ip);

	if (!(inode->i_state & I_CLEAR)) {
		ip->i_d.di_atime.t_sec = (__int32_t)inode->i_atime.tv_sec;
		ip->i_d.di_atime.t_nsec = (__int32_t)inode->i_atime.tv_nsec;
	}
}

void
xfs_mark_inode_dirty_sync(
	xfs_inode_t	*ip)
{
	struct inode	*inode = VFS_I(ip);

	if (!(inode->i_state & (I_WILL_FREE|I_FREEING|I_CLEAR)))
		mark_inode_dirty_sync(inode);
}

void
xfs_ichgtime(
	xfs_inode_t	*ip,
	int		flags)
{
	struct inode	*inode = VFS_I(ip);
	timespec_t	tv;
	int		sync_it = 0;

	tv = current_fs_time(inode->i_sb);

	if ((flags & XFS_ICHGTIME_MOD) &&
	    !timespec_equal(&inode->i_mtime, &tv)) {
		inode->i_mtime = tv;
		ip->i_d.di_mtime.t_sec = (__int32_t)tv.tv_sec;
		ip->i_d.di_mtime.t_nsec = (__int32_t)tv.tv_nsec;
		sync_it = 1;
	}
	if ((flags & XFS_ICHGTIME_CHG) &&
	    !timespec_equal(&inode->i_ctime, &tv)) {
		inode->i_ctime = tv;
		ip->i_d.di_ctime.t_sec = (__int32_t)tv.tv_sec;
		ip->i_d.di_ctime.t_nsec = (__int32_t)tv.tv_nsec;
		sync_it = 1;
	}

	/*
	 * We update the i_update_core field _after_ changing
	 * the timestamps in order to coordinate properly with
	 * xfs_iflush() so that we don't lose timestamp updates.
	 * This keeps us from having to hold the inode lock
	 * while doing this.  We use the SYNCHRONIZE macro to
	 * ensure that the compiler does not reorder the update
	 * of i_update_core above the timestamp updates above.
	 */
	if (sync_it) {
		SYNCHRONIZE();
		ip->i_update_core = 1;
		xfs_mark_inode_dirty_sync(ip);
	}
}

STATIC int
xfs_init_security(
	struct inode	*inode,
	struct inode	*dir)
{
	struct xfs_inode *ip = XFS_I(inode);
	size_t		length;
	void		*value;
	char		*name;
	int		error;

	error = security_inode_init_security(inode, dir, &name,
					     &value, &length);
	if (error) {
		if (error == -EOPNOTSUPP)
			return 0;
		return -error;
	}

	error = xfs_attr_set(ip, name, value, length, ATTR_SECURE);

	kfree(name);
	kfree(value);
	return error;
}

static void
xfs_dentry_to_name(
	struct xfs_name	*namep,
	struct dentry	*dentry)
{
	namep->name = dentry->d_name.name;
	namep->len = dentry->d_name.len;
}

STATIC void
xfs_cleanup_inode(
	struct inode	*dir,
	struct inode	*inode,
	struct dentry	*dentry)
{
	struct xfs_name	teardown;

	/* Oh, the horror.
	 * If we can't add the ACL or we fail in
	 * xfs_init_security we must back out.
	 * ENOSPC can hit here, among other things.
	 */
	xfs_dentry_to_name(&teardown, dentry);

	xfs_remove(XFS_I(dir), &teardown, XFS_I(inode));
	iput(inode);
}

STATIC int
xfs_vn_mknod(
	struct inode	*dir,
	struct dentry	*dentry,
	int		mode,
	dev_t		rdev)
{
	struct inode	*inode;
	struct xfs_inode *ip = NULL;
	xfs_acl_t	*default_acl = NULL;
	struct xfs_name	name;
	int (*test_default_acl)(struct inode *) = _ACL_DEFAULT_EXISTS;
	int		error;

	/*
	 * Irix uses Missed'em'V split, but doesn't want to see
	 * the upper 5 bits of (14bit) major.
	 */
	if (unlikely(!sysv_valid_dev(rdev) || MAJOR(rdev) & ~0x1ff))
		return -EINVAL;

	if (test_default_acl && test_default_acl(dir)) {
		if (!_ACL_ALLOC(default_acl)) {
			return -ENOMEM;
		}
		if (!_ACL_GET_DEFAULT(dir, default_acl)) {
			_ACL_FREE(default_acl);
			default_acl = NULL;
		}
	}

	xfs_dentry_to_name(&name, dentry);

	if (IS_POSIXACL(dir) && !default_acl)
		mode &= ~current->fs->umask;

	switch (mode & S_IFMT) {
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFSOCK:
		rdev = sysv_encode_dev(rdev);
	case S_IFREG:
		error = xfs_create(XFS_I(dir), &name, mode, rdev, &ip, NULL);
		break;
	case S_IFDIR:
		error = xfs_mkdir(XFS_I(dir), &name, mode, &ip, NULL);
		break;
	default:
		error = EINVAL;
		break;
	}

	if (unlikely(error))
		goto out_free_acl;

	inode = VFS_I(ip);

	error = xfs_init_security(inode, dir);
	if (unlikely(error))
		goto out_cleanup_inode;

	if (default_acl) {
		error = _ACL_INHERIT(inode, mode, default_acl);
		if (unlikely(error))
			goto out_cleanup_inode;
		_ACL_FREE(default_acl);
	}


	d_instantiate(dentry, inode);
	return -error;

 out_cleanup_inode:
	xfs_cleanup_inode(dir, inode, dentry);
 out_free_acl:
	if (default_acl)
		_ACL_FREE(default_acl);
	return -error;
}

STATIC int
xfs_vn_create(
	struct inode	*dir,
	struct dentry	*dentry,
	int		mode,
	struct nameidata *nd)
{
	return xfs_vn_mknod(dir, dentry, mode, 0);
}

STATIC int
xfs_vn_mkdir(
	struct inode	*dir,
	struct dentry	*dentry,
	int		mode)
{
	return xfs_vn_mknod(dir, dentry, mode|S_IFDIR, 0);
}

STATIC struct dentry *
xfs_vn_lookup(
	struct inode	*dir,
	struct dentry	*dentry,
	struct nameidata *nd)
{
	struct xfs_inode *cip;
	struct xfs_name	name;
	int		error;

	if (dentry->d_name.len >= MAXNAMELEN)
		return ERR_PTR(-ENAMETOOLONG);

	xfs_dentry_to_name(&name, dentry);
	error = xfs_lookup(XFS_I(dir), &name, &cip, NULL);
	if (unlikely(error)) {
		if (unlikely(error != ENOENT))
			return ERR_PTR(-error);
		d_add(dentry, NULL);
		return NULL;
	}

	return d_splice_alias(VFS_I(cip), dentry);
}

STATIC struct dentry *
xfs_vn_ci_lookup(
	struct inode	*dir,
	struct dentry	*dentry,
	struct nameidata *nd)
{
	struct xfs_inode *ip;
	struct xfs_name	xname;
	struct xfs_name ci_name;
	struct qstr	dname;
	int		error;

	if (dentry->d_name.len >= MAXNAMELEN)
		return ERR_PTR(-ENAMETOOLONG);

	xfs_dentry_to_name(&xname, dentry);
	error = xfs_lookup(XFS_I(dir), &xname, &ip, &ci_name);
	if (unlikely(error)) {
		if (unlikely(error != ENOENT))
			return ERR_PTR(-error);
		/*
		 * call d_add(dentry, NULL) here when d_drop_negative_children
		 * is called in xfs_vn_mknod (ie. allow negative dentries
		 * with CI filesystems).
		 */
		return NULL;
	}

	/* if exact match, just splice and exit */
	if (!ci_name.name)
		return d_splice_alias(VFS_I(ip), dentry);

	/* else case-insensitive match... */
	dname.name = ci_name.name;
	dname.len = ci_name.len;
	dentry = d_add_ci(dentry, VFS_I(ip), &dname);
	kmem_free(ci_name.name);
	return dentry;
}

STATIC int
xfs_vn_link(
	struct dentry	*old_dentry,
	struct inode	*dir,
	struct dentry	*dentry)
{
	struct inode	*inode = old_dentry->d_inode;
	struct xfs_name	name;
	int		error;

	xfs_dentry_to_name(&name, dentry);

	error = xfs_link(XFS_I(dir), XFS_I(inode), &name);
	if (unlikely(error))
		return -error;

	atomic_inc(&inode->i_count);
	d_instantiate(dentry, inode);
	return 0;
}

STATIC int
xfs_vn_unlink(
	struct inode	*dir,
	struct dentry	*dentry)
{
	struct xfs_name	name;
	int		error;

	xfs_dentry_to_name(&name, dentry);

	error = -xfs_remove(XFS_I(dir), &name, XFS_I(dentry->d_inode));
	if (error)
		return error;

	/*
	 * With unlink, the VFS makes the dentry "negative": no inode,
	 * but still hashed. This is incompatible with case-insensitive
	 * mode, so invalidate (unhash) the dentry in CI-mode.
	 */
	if (xfs_sb_version_hasasciici(&XFS_M(dir->i_sb)->m_sb))
		d_invalidate(dentry);
	return 0;
}

STATIC int
xfs_vn_symlink(
	struct inode	*dir,
	struct dentry	*dentry,
	const char	*symname)
{
	struct inode	*inode;
	struct xfs_inode *cip = NULL;
	struct xfs_name	name;
	int		error;
	mode_t		mode;

	mode = S_IFLNK |
		(irix_symlink_mode ? 0777 & ~current->fs->umask : S_IRWXUGO);
	xfs_dentry_to_name(&name, dentry);

	error = xfs_symlink(XFS_I(dir), &name, symname, mode, &cip, NULL);
	if (unlikely(error))
		goto out;

	inode = VFS_I(cip);

	error = xfs_init_security(inode, dir);
	if (unlikely(error))
		goto out_cleanup_inode;

	d_instantiate(dentry, inode);
	return 0;

 out_cleanup_inode:
	xfs_cleanup_inode(dir, inode, dentry);
 out:
	return -error;
}

STATIC int
xfs_vn_rename(
	struct inode	*odir,
	struct dentry	*odentry,
	struct inode	*ndir,
	struct dentry	*ndentry)
{
	struct inode	*new_inode = ndentry->d_inode;
	struct xfs_name	oname;
	struct xfs_name	nname;

	xfs_dentry_to_name(&oname, odentry);
	xfs_dentry_to_name(&nname, ndentry);

	return -xfs_rename(XFS_I(odir), &oname, XFS_I(odentry->d_inode),
			   XFS_I(ndir), &nname, new_inode ?
			   			XFS_I(new_inode) : NULL);
}

STATIC void *
xfs_vn_follow_link(
	struct dentry		*dentry,
	struct nameidata	*nd)
{
	char			*link;
	int			error = -ENOMEM;

	link = kmalloc(MAXPATHLEN+1, GFP_KERNEL);
	if (!link)
		goto out_err;

	error = -xfs_readlink(XFS_I(dentry->d_inode), link);
	if (unlikely(error))
		goto out_kfree;

	nd_set_link(nd, link);
	return NULL;

 out_kfree:
	kfree(link);
 out_err:
	nd_set_link(nd, ERR_PTR(error));
	return NULL;
}

STATIC void
xfs_vn_put_link(
	struct dentry	*dentry,
	struct nameidata *nd,
	void		*p)
{
	char		*s = nd_get_link(nd);

	if (!IS_ERR(s))
		kfree(s);
}

#ifdef CONFIG_XFS_POSIX_ACL
STATIC int
xfs_check_acl(
	struct inode		*inode,
	int			mask)
{
	struct xfs_inode	*ip = XFS_I(inode);
	int			error;

	xfs_itrace_entry(ip);

	if (XFS_IFORK_Q(ip)) {
		error = xfs_acl_iaccess(ip, mask, NULL);
		if (error != -1)
			return -error;
	}

	return -EAGAIN;
}

STATIC int
xfs_vn_permission(
	struct inode		*inode,
	int			mask)
{
	return generic_permission(inode, mask, xfs_check_acl);
}
#else
#define xfs_vn_permission NULL
#endif

STATIC int
xfs_vn_getattr(
	struct vfsmount		*mnt,
	struct dentry		*dentry,
	struct kstat		*stat)
{
	struct inode		*inode = dentry->d_inode;
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;

	xfs_itrace_entry(ip);

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	stat->size = XFS_ISIZE(ip);
	stat->dev = inode->i_sb->s_dev;
	stat->mode = ip->i_d.di_mode;
	stat->nlink = ip->i_d.di_nlink;
	stat->uid = ip->i_d.di_uid;
	stat->gid = ip->i_d.di_gid;
	stat->ino = ip->i_ino;
#if XFS_BIG_INUMS
	stat->ino += mp->m_inoadd;
#endif
	stat->atime = inode->i_atime;
	stat->mtime.tv_sec = ip->i_d.di_mtime.t_sec;
	stat->mtime.tv_nsec = ip->i_d.di_mtime.t_nsec;
	stat->ctime.tv_sec = ip->i_d.di_ctime.t_sec;
	stat->ctime.tv_nsec = ip->i_d.di_ctime.t_nsec;
	stat->blocks =
		XFS_FSB_TO_BB(mp, ip->i_d.di_nblocks + ip->i_delayed_blks);


	switch (inode->i_mode & S_IFMT) {
	case S_IFBLK:
	case S_IFCHR:
		stat->blksize = BLKDEV_IOSIZE;
		stat->rdev = MKDEV(sysv_major(ip->i_df.if_u2.if_rdev) & 0x1ff,
				   sysv_minor(ip->i_df.if_u2.if_rdev));
		break;
	default:
		if (XFS_IS_REALTIME_INODE(ip)) {
			/*
			 * If the file blocks are being allocated from a
			 * realtime volume, then return the inode's realtime
			 * extent size or the realtime volume's extent size.
			 */
			stat->blksize =
				xfs_get_extsz_hint(ip) << mp->m_sb.sb_blocklog;
		} else
			stat->blksize = xfs_preferred_iosize(mp);
		stat->rdev = 0;
		break;
	}

	return 0;
}

STATIC int
xfs_vn_setattr(
	struct dentry	*dentry,
	struct iattr	*iattr)
{
	return -xfs_setattr(XFS_I(dentry->d_inode), iattr, 0);
}

STATIC void
xfs_vn_truncate(
	struct inode	*inode)
{
	int	error;
	error = block_truncate_page(inode->i_mapping, inode->i_size,
							xfs_get_blocks);
	WARN_ON(error);
}

STATIC long
xfs_vn_fallocate(
	struct inode	*inode,
	int		mode,
	loff_t		offset,
	loff_t		len)
{
	long		error;
	loff_t		new_size = 0;
	xfs_flock64_t	bf;
	xfs_inode_t	*ip = XFS_I(inode);

	/* preallocation on directories not yet supported */
	error = -ENODEV;
	if (S_ISDIR(inode->i_mode))
		goto out_error;

	bf.l_whence = 0;
	bf.l_start = offset;
	bf.l_len = len;

	xfs_ilock(ip, XFS_IOLOCK_EXCL);
	error = xfs_change_file_space(ip, XFS_IOC_RESVSP, &bf,
				      0, XFS_ATTR_NOLOCK);
	if (!error && !(mode & FALLOC_FL_KEEP_SIZE) &&
	    offset + len > i_size_read(inode))
		new_size = offset + len;

	/* Change file size if needed */
	if (new_size) {
		struct iattr iattr;

		iattr.ia_valid = ATTR_SIZE;
		iattr.ia_size = new_size;
		error = xfs_setattr(ip, &iattr, XFS_ATTR_NOLOCK);
	}

	xfs_iunlock(ip, XFS_IOLOCK_EXCL);
out_error:
	return error;
}

#define XFS_FIEMAP_FLAGS	(FIEMAP_FLAG_SYNC|FIEMAP_FLAG_XATTR)

STATIC int
xfs_fiemap_format(
	void			**arg,
	struct getbmapx		*bmv,
	int			*full)
{
	int			error;
	struct fiemap_extent_info *fieinfo = *arg;
	u32			fiemap_flags = 0;
	u64			logical, physical, length;

	/* Do nothing for a hole */
	if (bmv->bmv_block == -1LL)
		return 0;

	logical = BBTOB(bmv->bmv_offset);
	physical = BBTOB(bmv->bmv_block);
	length = BBTOB(bmv->bmv_length);

	if (bmv->bmv_oflags & BMV_OF_PREALLOC)
		fiemap_flags |= FIEMAP_EXTENT_UNWRITTEN;
	else if (bmv->bmv_oflags & BMV_OF_DELALLOC) {
		fiemap_flags |= FIEMAP_EXTENT_DELALLOC;
		physical = 0;   /* no block yet */
	}
	if (bmv->bmv_oflags & BMV_OF_LAST)
		fiemap_flags |= FIEMAP_EXTENT_LAST;

	error = fiemap_fill_next_extent(fieinfo, logical, physical,
					length, fiemap_flags);
	if (error > 0) {
		error = 0;
		*full = 1;	/* user array now full */
	}

	return -error;
}

STATIC int
xfs_vn_fiemap(
	struct inode		*inode,
	struct fiemap_extent_info *fieinfo,
	u64			start,
	u64			length)
{
	xfs_inode_t		*ip = XFS_I(inode);
	struct getbmapx		bm;
	int			error;

	error = fiemap_check_flags(fieinfo, XFS_FIEMAP_FLAGS);
	if (error)
		return error;

	/* Set up bmap header for xfs internal routine */
	bm.bmv_offset = BTOBB(start);
	/* Special case for whole file */
	if (length == FIEMAP_MAX_OFFSET)
		bm.bmv_length = -1LL;
	else
		bm.bmv_length = BTOBB(length);

	/* our formatter will tell xfs_getbmap when to stop. */
	bm.bmv_count = MAXEXTNUM;
	bm.bmv_iflags = BMV_IF_PREALLOC;
	if (fieinfo->fi_flags & FIEMAP_FLAG_XATTR)
		bm.bmv_iflags |= BMV_IF_ATTRFORK;
	if (!(fieinfo->fi_flags & FIEMAP_FLAG_SYNC))
		bm.bmv_iflags |= BMV_IF_DELALLOC;

	error = xfs_getbmap(ip, &bm, xfs_fiemap_format, fieinfo);
	if (error)
		return -error;

	return 0;
}

static const struct inode_operations xfs_inode_operations = {
	.permission		= xfs_vn_permission,
	.truncate		= xfs_vn_truncate,
	.getattr		= xfs_vn_getattr,
	.setattr		= xfs_vn_setattr,
	.setxattr		= generic_setxattr,
	.getxattr		= generic_getxattr,
	.removexattr		= generic_removexattr,
	.listxattr		= xfs_vn_listxattr,
	.fallocate		= xfs_vn_fallocate,
	.fiemap			= xfs_vn_fiemap,
};

static const struct inode_operations xfs_dir_inode_operations = {
	.create			= xfs_vn_create,
	.lookup			= xfs_vn_lookup,
	.link			= xfs_vn_link,
	.unlink			= xfs_vn_unlink,
	.symlink		= xfs_vn_symlink,
	.mkdir			= xfs_vn_mkdir,
	/*
	 * Yes, XFS uses the same method for rmdir and unlink.
	 *
	 * There are some subtile differences deeper in the code,
	 * but we use S_ISDIR to check for those.
	 */
	.rmdir			= xfs_vn_unlink,
	.mknod			= xfs_vn_mknod,
	.rename			= xfs_vn_rename,
	.permission		= xfs_vn_permission,
	.getattr		= xfs_vn_getattr,
	.setattr		= xfs_vn_setattr,
	.setxattr		= generic_setxattr,
	.getxattr		= generic_getxattr,
	.removexattr		= generic_removexattr,
	.listxattr		= xfs_vn_listxattr,
};

static const struct inode_operations xfs_dir_ci_inode_operations = {
	.create			= xfs_vn_create,
	.lookup			= xfs_vn_ci_lookup,
	.link			= xfs_vn_link,
	.unlink			= xfs_vn_unlink,
	.symlink		= xfs_vn_symlink,
	.mkdir			= xfs_vn_mkdir,
	/*
	 * Yes, XFS uses the same method for rmdir and unlink.
	 *
	 * There are some subtile differences deeper in the code,
	 * but we use S_ISDIR to check for those.
	 */
	.rmdir			= xfs_vn_unlink,
	.mknod			= xfs_vn_mknod,
	.rename			= xfs_vn_rename,
	.permission		= xfs_vn_permission,
	.getattr		= xfs_vn_getattr,
	.setattr		= xfs_vn_setattr,
	.setxattr		= generic_setxattr,
	.getxattr		= generic_getxattr,
	.removexattr		= generic_removexattr,
	.listxattr		= xfs_vn_listxattr,
};

static const struct inode_operations xfs_symlink_inode_operations = {
	.readlink		= generic_readlink,
	.follow_link		= xfs_vn_follow_link,
	.put_link		= xfs_vn_put_link,
	.permission		= xfs_vn_permission,
	.getattr		= xfs_vn_getattr,
	.setattr		= xfs_vn_setattr,
	.setxattr		= generic_setxattr,
	.getxattr		= generic_getxattr,
	.removexattr		= generic_removexattr,
	.listxattr		= xfs_vn_listxattr,
};

STATIC void
xfs_diflags_to_iflags(
	struct inode		*inode,
	struct xfs_inode	*ip)
{
	if (ip->i_d.di_flags & XFS_DIFLAG_IMMUTABLE)
		inode->i_flags |= S_IMMUTABLE;
	else
		inode->i_flags &= ~S_IMMUTABLE;
	if (ip->i_d.di_flags & XFS_DIFLAG_APPEND)
		inode->i_flags |= S_APPEND;
	else
		inode->i_flags &= ~S_APPEND;
	if (ip->i_d.di_flags & XFS_DIFLAG_SYNC)
		inode->i_flags |= S_SYNC;
	else
		inode->i_flags &= ~S_SYNC;
	if (ip->i_d.di_flags & XFS_DIFLAG_NOATIME)
		inode->i_flags |= S_NOATIME;
	else
		inode->i_flags &= ~S_NOATIME;
}

void
xfs_setup_inode(
	struct xfs_inode	*ip)
{
	struct inode		*inode = &ip->i_vnode;

	inode->i_ino = ip->i_ino;
	inode->i_state = I_NEW|I_LOCK;
	inode_add_to_lists(ip->i_mount->m_super, inode);

	inode->i_mode	= ip->i_d.di_mode;
	inode->i_nlink	= ip->i_d.di_nlink;
	inode->i_uid	= ip->i_d.di_uid;
	inode->i_gid	= ip->i_d.di_gid;

	switch (inode->i_mode & S_IFMT) {
	case S_IFBLK:
	case S_IFCHR:
		inode->i_rdev =
			MKDEV(sysv_major(ip->i_df.if_u2.if_rdev) & 0x1ff,
			      sysv_minor(ip->i_df.if_u2.if_rdev));
		break;
	default:
		inode->i_rdev = 0;
		break;
	}

	inode->i_generation = ip->i_d.di_gen;
	i_size_write(inode, ip->i_d.di_size);
	inode->i_atime.tv_sec	= ip->i_d.di_atime.t_sec;
	inode->i_atime.tv_nsec	= ip->i_d.di_atime.t_nsec;
	inode->i_mtime.tv_sec	= ip->i_d.di_mtime.t_sec;
	inode->i_mtime.tv_nsec	= ip->i_d.di_mtime.t_nsec;
	inode->i_ctime.tv_sec	= ip->i_d.di_ctime.t_sec;
	inode->i_ctime.tv_nsec	= ip->i_d.di_ctime.t_nsec;
	xfs_diflags_to_iflags(inode, ip);

	switch (inode->i_mode & S_IFMT) {
	case S_IFREG:
		inode->i_op = &xfs_inode_operations;
		inode->i_fop = &xfs_file_operations;
		inode->i_mapping->a_ops = &xfs_address_space_operations;
		break;
	case S_IFDIR:
		if (xfs_sb_version_hasasciici(&XFS_M(inode->i_sb)->m_sb))
			inode->i_op = &xfs_dir_ci_inode_operations;
		else
			inode->i_op = &xfs_dir_inode_operations;
		inode->i_fop = &xfs_dir_file_operations;
		break;
	case S_IFLNK:
		inode->i_op = &xfs_symlink_inode_operations;
		if (!(ip->i_df.if_flags & XFS_IFINLINE))
			inode->i_mapping->a_ops = &xfs_address_space_operations;
		break;
	default:
		inode->i_op = &xfs_inode_operations;
		init_special_inode(inode, inode->i_mode, inode->i_rdev);
		break;
	}

	xfs_iflags_clear(ip, XFS_INEW);
	barrier();

	unlock_new_inode(inode);
}