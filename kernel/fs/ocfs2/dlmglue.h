


#ifndef DLMGLUE_H
#define DLMGLUE_H

#include "dcache.h"

#define OCFS2_LVB_VERSION 5

struct ocfs2_meta_lvb {
	__u8         lvb_version;
	__u8         lvb_reserved0;
	__be16       lvb_idynfeatures;
	__be32       lvb_iclusters;
	__be32       lvb_iuid;
	__be32       lvb_igid;
	__be64       lvb_iatime_packed;
	__be64       lvb_ictime_packed;
	__be64       lvb_imtime_packed;
	__be64       lvb_isize;
	__be16       lvb_imode;
	__be16       lvb_inlink;
	__be32       lvb_iattr;
	__be32       lvb_igeneration;
	__be32       lvb_reserved2;
};

#define OCFS2_QINFO_LVB_VERSION 1

struct ocfs2_qinfo_lvb {
	__u8	lvb_version;
	__u8	lvb_reserved[3];
	__be32	lvb_bgrace;
	__be32	lvb_igrace;
	__be32	lvb_syncms;
	__be32	lvb_blocks;
	__be32	lvb_free_blk;
	__be32	lvb_free_entry;
};

/* ocfs2_inode_lock_full() 'arg_flags' flags */
/* don't wait on recovery. */
#define OCFS2_META_LOCK_RECOVERY	(0x01)
/* Instruct the dlm not to queue ourselves on the other node. */
#define OCFS2_META_LOCK_NOQUEUE		(0x02)
/* don't block waiting for the downconvert thread, instead return -EAGAIN */
#define OCFS2_LOCK_NONBLOCK		(0x04)

int ocfs2_dlm_init(struct ocfs2_super *osb);
void ocfs2_dlm_shutdown(struct ocfs2_super *osb, int hangup_pending);
void ocfs2_lock_res_init_once(struct ocfs2_lock_res *res);
void ocfs2_inode_lock_res_init(struct ocfs2_lock_res *res,
			       enum ocfs2_lock_type type,
			       unsigned int generation,
			       struct inode *inode);
void ocfs2_dentry_lock_res_init(struct ocfs2_dentry_lock *dl,
				u64 parent, struct inode *inode);
struct ocfs2_file_private;
void ocfs2_file_lock_res_init(struct ocfs2_lock_res *lockres,
			      struct ocfs2_file_private *fp);
struct ocfs2_mem_dqinfo;
void ocfs2_qinfo_lock_res_init(struct ocfs2_lock_res *lockres,
                               struct ocfs2_mem_dqinfo *info);
void ocfs2_lock_res_free(struct ocfs2_lock_res *res);
int ocfs2_create_new_inode_locks(struct inode *inode);
int ocfs2_drop_inode_locks(struct inode *inode);
int ocfs2_rw_lock(struct inode *inode, int write);
void ocfs2_rw_unlock(struct inode *inode, int write);
int ocfs2_open_lock(struct inode *inode);
int ocfs2_try_open_lock(struct inode *inode, int write);
void ocfs2_open_unlock(struct inode *inode);
int ocfs2_inode_lock_atime(struct inode *inode,
			  struct vfsmount *vfsmnt,
			  int *level);
int ocfs2_inode_lock_full(struct inode *inode,
			 struct buffer_head **ret_bh,
			 int ex,
			 int arg_flags);
int ocfs2_inode_lock_with_page(struct inode *inode,
			      struct buffer_head **ret_bh,
			      int ex,
			      struct page *page);
#define ocfs2_inode_lock(i, b, e) ocfs2_inode_lock_full(i, b, e, 0)
void ocfs2_inode_unlock(struct inode *inode,
		       int ex);
int ocfs2_super_lock(struct ocfs2_super *osb,
		     int ex);
void ocfs2_super_unlock(struct ocfs2_super *osb,
			int ex);
int ocfs2_rename_lock(struct ocfs2_super *osb);
void ocfs2_rename_unlock(struct ocfs2_super *osb);
int ocfs2_dentry_lock(struct dentry *dentry, int ex);
void ocfs2_dentry_unlock(struct dentry *dentry, int ex);
int ocfs2_file_lock(struct file *file, int ex, int trylock);
void ocfs2_file_unlock(struct file *file);
int ocfs2_qinfo_lock(struct ocfs2_mem_dqinfo *oinfo, int ex);
void ocfs2_qinfo_unlock(struct ocfs2_mem_dqinfo *oinfo, int ex);


void ocfs2_mark_lockres_freeing(struct ocfs2_lock_res *lockres);
void ocfs2_simple_drop_lockres(struct ocfs2_super *osb,
			       struct ocfs2_lock_res *lockres);

/* for the downconvert thread */
void ocfs2_wake_downconvert_thread(struct ocfs2_super *osb);

struct ocfs2_dlm_debug *ocfs2_new_dlm_debug(void);
void ocfs2_put_dlm_debug(struct ocfs2_dlm_debug *dlm_debug);

/* To set the locking protocol on module initialization */
void ocfs2_set_locking_protocol(void);
#endif	/* DLMGLUE_H */
