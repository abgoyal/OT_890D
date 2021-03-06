

#ifndef OCFS2_DIR_H
#define OCFS2_DIR_H

struct buffer_head *ocfs2_find_entry(const char *name,
				     int namelen,
				     struct inode *dir,
				     struct ocfs2_dir_entry **res_dir);
int ocfs2_delete_entry(handle_t *handle,
		       struct inode *dir,
		       struct ocfs2_dir_entry *de_del,
		       struct buffer_head *bh);
int __ocfs2_add_entry(handle_t *handle,
		      struct inode *dir,
		      const char *name, int namelen,
		      struct inode *inode, u64 blkno,
		      struct buffer_head *parent_fe_bh,
		      struct buffer_head *insert_bh);
static inline int ocfs2_add_entry(handle_t *handle,
				  struct dentry *dentry,
				  struct inode *inode, u64 blkno,
				  struct buffer_head *parent_fe_bh,
				  struct buffer_head *insert_bh)
{
	return __ocfs2_add_entry(handle, dentry->d_parent->d_inode,
				 dentry->d_name.name, dentry->d_name.len,
				 inode, blkno, parent_fe_bh, insert_bh);
}
int ocfs2_update_entry(struct inode *dir, handle_t *handle,
		       struct buffer_head *de_bh, struct ocfs2_dir_entry *de,
		       struct inode *new_entry_inode);

int ocfs2_check_dir_for_entry(struct inode *dir,
			      const char *name,
			      int namelen);
int ocfs2_empty_dir(struct inode *inode);
int ocfs2_find_files_on_disk(const char *name,
			     int namelen,
			     u64 *blkno,
			     struct inode *inode,
			     struct buffer_head **dirent_bh,
			     struct ocfs2_dir_entry **dirent);
int ocfs2_lookup_ino_from_name(struct inode *dir, const char *name,
			       int namelen, u64 *blkno);
int ocfs2_readdir(struct file *filp, void *dirent, filldir_t filldir);
int ocfs2_dir_foreach(struct inode *inode, loff_t *f_pos, void *priv,
		      filldir_t filldir);
int ocfs2_prepare_dir_for_insert(struct ocfs2_super *osb,
				 struct inode *dir,
				 struct buffer_head *parent_fe_bh,
				 const char *name,
				 int namelen,
				 struct buffer_head **ret_de_bh);
struct ocfs2_alloc_context;
int ocfs2_fill_new_dir(struct ocfs2_super *osb,
		       handle_t *handle,
		       struct inode *parent,
		       struct inode *inode,
		       struct buffer_head *fe_bh,
		       struct ocfs2_alloc_context *data_ac);

struct ocfs2_dir_block_trailer *ocfs2_dir_trailer_from_size(int blocksize,
							    void *data);
#endif /* OCFS2_DIR_H */
