EXTRA_CFLAGS +=	 -I$(src) -I$(src)/linux-2.6 -funsigned-char

XFS_LINUX := linux-2.6

ifeq ($(CONFIG_XFS_DEBUG),y)
	EXTRA_CFLAGS += -g
endif

obj-$(CONFIG_XFS_FS)		+= xfs.o

xfs-$(CONFIG_XFS_QUOTA)		+= $(addprefix quota/, \
				   xfs_dquot.o \
				   xfs_dquot_item.o \
				   xfs_trans_dquot.o \
				   xfs_qm_syscalls.o \
				   xfs_qm_bhv.o \
				   xfs_qm.o)

ifeq ($(CONFIG_XFS_QUOTA),y)
xfs-$(CONFIG_PROC_FS)		+= quota/xfs_qm_stats.o
endif

xfs-$(CONFIG_XFS_RT)		+= xfs_rtalloc.o
xfs-$(CONFIG_XFS_POSIX_ACL)	+= xfs_acl.o
xfs-$(CONFIG_PROC_FS)		+= $(XFS_LINUX)/xfs_stats.o
xfs-$(CONFIG_SYSCTL)		+= $(XFS_LINUX)/xfs_sysctl.o
xfs-$(CONFIG_COMPAT)		+= $(XFS_LINUX)/xfs_ioctl32.o


xfs-y				+= xfs_alloc.o \
				   xfs_alloc_btree.o \
				   xfs_attr.o \
				   xfs_attr_leaf.o \
				   xfs_bit.o \
				   xfs_bmap.o \
				   xfs_bmap_btree.o \
				   xfs_btree.o \
				   xfs_buf_item.o \
				   xfs_da_btree.o \
				   xfs_dir2.o \
				   xfs_dir2_block.o \
				   xfs_dir2_data.o \
				   xfs_dir2_leaf.o \
				   xfs_dir2_node.o \
				   xfs_dir2_sf.o \
				   xfs_error.o \
				   xfs_extfree_item.o \
				   xfs_filestream.o \
				   xfs_fsops.o \
				   xfs_ialloc.o \
				   xfs_ialloc_btree.o \
				   xfs_iget.o \
				   xfs_inode.o \
				   xfs_inode_item.o \
				   xfs_iomap.o \
				   xfs_itable.o \
				   xfs_dfrag.o \
				   xfs_log.o \
				   xfs_log_recover.o \
				   xfs_mount.o \
				   xfs_mru_cache.o \
				   xfs_rename.o \
				   xfs_trans.o \
				   xfs_trans_ail.o \
				   xfs_trans_buf.o \
				   xfs_trans_extfree.o \
				   xfs_trans_inode.o \
				   xfs_trans_item.o \
				   xfs_utils.o \
				   xfs_vnodeops.o \
				   xfs_rw.o \
				   xfs_dmops.o \
				   xfs_qmops.o

xfs-$(CONFIG_XFS_TRACE)		+= xfs_btree_trace.o \
				   xfs_dir2_trace.o

# Objects in linux/
xfs-y				+= $(addprefix $(XFS_LINUX)/, \
				   kmem.o \
				   xfs_aops.o \
				   xfs_buf.o \
				   xfs_export.o \
				   xfs_file.o \
				   xfs_fs_subr.o \
				   xfs_globals.o \
				   xfs_ioctl.o \
				   xfs_iops.o \
				   xfs_lrw.o \
				   xfs_super.o \
				   xfs_sync.o \
				   xfs_xattr.o)

# Objects in support/
xfs-y				+= $(addprefix support/, \
				   debug.o \
				   uuid.o)

xfs-$(CONFIG_XFS_TRACE)		+= support/ktrace.o

