

#ifndef OCFS2_JOURNAL_H
#define OCFS2_JOURNAL_H

#include <linux/fs.h>
#include <linux/jbd2.h>

enum ocfs2_journal_state {
	OCFS2_JOURNAL_FREE = 0,
	OCFS2_JOURNAL_LOADED,
	OCFS2_JOURNAL_IN_SHUTDOWN,
};

struct ocfs2_super;
struct ocfs2_dinode;

struct ocfs2_journal {
	enum ocfs2_journal_state   j_state;    /* Journals current state   */

	journal_t                 *j_journal; /* The kernels journal type */
	struct inode              *j_inode;   /* Kernel inode pointing to
					       * this journal             */
	struct ocfs2_super        *j_osb;     /* pointer to the super
					       * block for the node
					       * we're currently
					       * running on -- not
					       * necessarily the super
					       * block from the node
					       * which we usually run
					       * from (recovery,
					       * etc)                     */
	struct buffer_head        *j_bh;      /* Journal disk inode block */
	atomic_t                  j_num_trans; /* Number of transactions
					        * currently in the system. */
	unsigned long             j_trans_id;
	struct rw_semaphore       j_trans_barrier;
	wait_queue_head_t         j_checkpointed;

	spinlock_t                j_lock;
	struct list_head          j_la_cleanups;
	struct work_struct        j_recovery_work;
};

extern spinlock_t trans_inc_lock;

/* wrap j_trans_id so we never have it equal to zero. */
static inline unsigned long ocfs2_inc_trans_id(struct ocfs2_journal *j)
{
	unsigned long old_id;
	spin_lock(&trans_inc_lock);
	old_id = j->j_trans_id++;
	if (unlikely(!j->j_trans_id))
		j->j_trans_id = 1;
	spin_unlock(&trans_inc_lock);
	return old_id;
}

static inline void ocfs2_set_inode_lock_trans(struct ocfs2_journal *journal,
					      struct inode *inode)
{
	spin_lock(&trans_inc_lock);
	OCFS2_I(inode)->ip_last_trans = journal->j_trans_id;
	spin_unlock(&trans_inc_lock);
}

static inline int ocfs2_inode_fully_checkpointed(struct inode *inode)
{
	int ret;
	struct ocfs2_journal *journal = OCFS2_SB(inode->i_sb)->journal;

	spin_lock(&trans_inc_lock);
	ret = time_after(journal->j_trans_id, OCFS2_I(inode)->ip_last_trans);
	spin_unlock(&trans_inc_lock);
	return ret;
}

static inline int ocfs2_inode_is_new(struct inode *inode)
{
	int ret;

	/* System files are never "new" as they're written out by
	 * mkfs. This helps us early during mount, before we have the
	 * journal open and j_trans_id could be junk. */
	if (OCFS2_I(inode)->ip_flags & OCFS2_INODE_SYSTEM_FILE)
		return 0;
	spin_lock(&trans_inc_lock);
	ret = !(time_after(OCFS2_SB(inode->i_sb)->journal->j_trans_id,
			   OCFS2_I(inode)->ip_created_trans));
	if (!ret)
		OCFS2_I(inode)->ip_created_trans = 0;
	spin_unlock(&trans_inc_lock);
	return ret;
}

static inline void ocfs2_inode_set_new(struct ocfs2_super *osb,
				       struct inode *inode)
{
	spin_lock(&trans_inc_lock);
	OCFS2_I(inode)->ip_created_trans = osb->journal->j_trans_id;
	spin_unlock(&trans_inc_lock);
}

/* Exported only for the journal struct init code in super.c. Do not call. */
void ocfs2_complete_recovery(struct work_struct *work);
void ocfs2_wait_for_recovery(struct ocfs2_super *osb);

int ocfs2_recovery_init(struct ocfs2_super *osb);
void ocfs2_recovery_exit(struct ocfs2_super *osb);

void   ocfs2_set_journal_params(struct ocfs2_super *osb);
int    ocfs2_journal_init(struct ocfs2_journal *journal,
			  int *dirty);
void   ocfs2_journal_shutdown(struct ocfs2_super *osb);
int    ocfs2_journal_wipe(struct ocfs2_journal *journal,
			  int full);
int    ocfs2_journal_load(struct ocfs2_journal *journal, int local,
			  int replayed);
int    ocfs2_check_journals_nolocks(struct ocfs2_super *osb);
void   ocfs2_recovery_thread(struct ocfs2_super *osb,
			     int node_num);
int    ocfs2_mark_dead_nodes(struct ocfs2_super *osb);
void   ocfs2_complete_mount_recovery(struct ocfs2_super *osb);
void ocfs2_complete_quota_recovery(struct ocfs2_super *osb);

static inline void ocfs2_start_checkpoint(struct ocfs2_super *osb)
{
	atomic_set(&osb->needs_checkpoint, 1);
	wake_up(&osb->checkpoint_event);
}

static inline void ocfs2_checkpoint_inode(struct inode *inode)
{
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);

	if (ocfs2_mount_local(osb))
		return;

	if (!ocfs2_inode_fully_checkpointed(inode)) {
		/* WARNING: This only kicks off a single
		 * checkpoint. If someone races you and adds more
		 * metadata to the journal, you won't know, and will
		 * wind up waiting *alot* longer than necessary. Right
		 * now we only use this in clear_inode so that's
		 * OK. */
		ocfs2_start_checkpoint(osb);

		wait_event(osb->journal->j_checkpointed,
			   ocfs2_inode_fully_checkpointed(inode));
	}
}


handle_t		    *ocfs2_start_trans(struct ocfs2_super *osb,
					       int max_buffs);
int			     ocfs2_commit_trans(struct ocfs2_super *osb,
						handle_t *handle);
int			     ocfs2_extend_trans(handle_t *handle, int nblocks);

#define OCFS2_JOURNAL_ACCESS_CREATE 0
#define OCFS2_JOURNAL_ACCESS_WRITE  1
#define OCFS2_JOURNAL_ACCESS_UNDO   2


/* ocfs2_inode */
int ocfs2_journal_access_di(handle_t *handle, struct inode *inode,
			    struct buffer_head *bh, int type);
/* ocfs2_extent_block */
int ocfs2_journal_access_eb(handle_t *handle, struct inode *inode,
			    struct buffer_head *bh, int type);
/* ocfs2_group_desc */
int ocfs2_journal_access_gd(handle_t *handle, struct inode *inode,
			    struct buffer_head *bh, int type);
/* ocfs2_xattr_block */
int ocfs2_journal_access_xb(handle_t *handle, struct inode *inode,
			    struct buffer_head *bh, int type);
/* quota blocks */
int ocfs2_journal_access_dq(handle_t *handle, struct inode *inode,
			    struct buffer_head *bh, int type);
/* dirblock */
int ocfs2_journal_access_db(handle_t *handle, struct inode *inode,
			    struct buffer_head *bh, int type);
/* Anything that has no ecc */
int ocfs2_journal_access(handle_t *handle, struct inode *inode,
			 struct buffer_head *bh, int type);

int                  ocfs2_journal_dirty(handle_t *handle,
					 struct buffer_head *bh);


/* simple file updates like chmod, etc. */
#define OCFS2_INODE_UPDATE_CREDITS 1

/* extended attribute block update */
#define OCFS2_XATTR_BLOCK_UPDATE_CREDITS 1

/* global quotafile inode update, data block */
#define OCFS2_QINFO_WRITE_CREDITS (OCFS2_INODE_UPDATE_CREDITS + 1)

/* quota data block, global info */
/* Write to local quota file */
#define OCFS2_QWRITE_CREDITS (OCFS2_QINFO_WRITE_CREDITS + 1)

#define OCFS2_QSYNC_CREDITS (OCFS2_INODE_UPDATE_CREDITS + 3)

static inline int ocfs2_quota_trans_credits(struct super_block *sb)
{
	int credits = 0;

	if (OCFS2_HAS_RO_COMPAT_FEATURE(sb, OCFS2_FEATURE_RO_COMPAT_USRQUOTA))
		credits += OCFS2_QWRITE_CREDITS;
	if (OCFS2_HAS_RO_COMPAT_FEATURE(sb, OCFS2_FEATURE_RO_COMPAT_GRPQUOTA))
		credits += OCFS2_QWRITE_CREDITS;
	return credits;
}

/* Number of credits needed for removing quota structure from file */
int ocfs2_calc_qdel_credits(struct super_block *sb, int type);
/* Number of credits needed for initialization of new quota structure */
int ocfs2_calc_qinit_credits(struct super_block *sb, int type);

/* group extend. inode update and last group update. */
#define OCFS2_GROUP_EXTEND_CREDITS	(OCFS2_INODE_UPDATE_CREDITS + 1)

/* group add. inode update and the new group update. */
#define OCFS2_GROUP_ADD_CREDITS	(OCFS2_INODE_UPDATE_CREDITS + 1)

#define OCFS2_SUBALLOC_ALLOC (3)

static inline int ocfs2_inline_to_extents_credits(struct super_block *sb)
{
	return OCFS2_SUBALLOC_ALLOC + OCFS2_INODE_UPDATE_CREDITS +
	       ocfs2_quota_trans_credits(sb);
}

/* dinode + group descriptor update. We don't relink on free yet. */
#define OCFS2_SUBALLOC_FREE  (2)

#define OCFS2_TRUNCATE_LOG_UPDATE OCFS2_INODE_UPDATE_CREDITS
#define OCFS2_TRUNCATE_LOG_FLUSH_ONE_REC (OCFS2_SUBALLOC_FREE 		      \
					 + OCFS2_TRUNCATE_LOG_UPDATE)

static inline int ocfs2_remove_extent_credits(struct super_block *sb)
{
	return OCFS2_TRUNCATE_LOG_UPDATE + OCFS2_INODE_UPDATE_CREDITS +
	       ocfs2_quota_trans_credits(sb);
}

#define OCFS2_DIR_LINK_ADDITIONAL_CREDITS (1 + 2)

static inline int ocfs2_mknod_credits(struct super_block *sb)
{
	return 3 + OCFS2_SUBALLOC_ALLOC + OCFS2_DIR_LINK_ADDITIONAL_CREDITS +
	       ocfs2_quota_trans_credits(sb);
}

/* local alloc metadata change + main bitmap updates */
#define OCFS2_WINDOW_MOVE_CREDITS (OCFS2_INODE_UPDATE_CREDITS                 \
				  + OCFS2_SUBALLOC_ALLOC + OCFS2_SUBALLOC_FREE)

#define OCFS2_SIMPLE_DIR_EXTEND_CREDITS (2)

static inline int ocfs2_link_credits(struct super_block *sb)
{
	return 2*OCFS2_INODE_UPDATE_CREDITS + 1 +
	       ocfs2_quota_trans_credits(sb);
}

static inline int ocfs2_unlink_credits(struct super_block *sb)
{
	/* The quota update from ocfs2_link_credits is unused here... */
	return 2 * OCFS2_INODE_UPDATE_CREDITS + 1 + ocfs2_link_credits(sb);
}

#define OCFS2_DELETE_INODE_CREDITS (3 * OCFS2_INODE_UPDATE_CREDITS + 1 + 1)

static inline int ocfs2_rename_credits(struct super_block *sb)
{
	return 3 * OCFS2_INODE_UPDATE_CREDITS + 3 + ocfs2_unlink_credits(sb);
}

#define OCFS2_XATTR_BLOCK_CREATE_CREDITS (OCFS2_SUBALLOC_ALLOC * 2 + \
					  + OCFS2_INODE_UPDATE_CREDITS \
					  + OCFS2_XATTR_BLOCK_UPDATE_CREDITS)

static inline int ocfs2_calc_extend_credits(struct super_block *sb,
					    struct ocfs2_extent_list *root_el,
					    u32 bits_wanted)
{
	int bitmap_blocks, sysfile_bitmap_blocks, extent_blocks;

	/* bitmap dinode, group desc. + relinked group. */
	bitmap_blocks = OCFS2_SUBALLOC_ALLOC;

	/* we might need to shift tree depth so lets assume an
	 * absolute worst case of complete fragmentation.  Even with
	 * that, we only need one update for the dinode, and then
	 * however many metadata chunks needed * a remaining suballoc
	 * alloc. */
	sysfile_bitmap_blocks = 1 +
		(OCFS2_SUBALLOC_ALLOC - 1) * ocfs2_extend_meta_needed(root_el);

	/* this does not include *new* metadata blocks, which are
	 * accounted for in sysfile_bitmap_blocks. root_el +
	 * prev. last_eb_blk + blocks along edge of tree.
	 * calc_symlink_credits passes because we just need 1
	 * credit for the dinode there. */
	extent_blocks = 1 + 1 + le16_to_cpu(root_el->l_tree_depth);

	return bitmap_blocks + sysfile_bitmap_blocks + extent_blocks +
	       ocfs2_quota_trans_credits(sb);
}

static inline int ocfs2_calc_symlink_credits(struct super_block *sb)
{
	int blocks = ocfs2_mknod_credits(sb);

	/* links can be longer than one block so we may update many
	 * within our single allocated extent. */
	blocks += ocfs2_clusters_to_blocks(sb, 1);

	return blocks + ocfs2_quota_trans_credits(sb);
}

static inline int ocfs2_calc_group_alloc_credits(struct super_block *sb,
						 unsigned int cpg)
{
	int blocks;
	int bitmap_blocks = OCFS2_SUBALLOC_ALLOC + 1;
	/* parent inode update + new block group header + bitmap inode update
	   + bitmap blocks affected */
	blocks = 1 + 1 + 1 + bitmap_blocks;
	return blocks;
}

static inline int ocfs2_calc_tree_trunc_credits(struct super_block *sb,
						unsigned int clusters_to_del,
						struct ocfs2_dinode *fe,
						struct ocfs2_extent_list *last_el)
{
 	/* for dinode + all headers in this pass + update to next leaf */
	u16 next_free = le16_to_cpu(last_el->l_next_free_rec);
	u16 tree_depth = le16_to_cpu(fe->id2.i_list.l_tree_depth);
	int credits = 1 + tree_depth + 1;
	int i;

	i = next_free - 1;
	BUG_ON(i < 0);

	/* We may be deleting metadata blocks, so metadata alloc dinode +
	   one desc. block for each possible delete. */
	if (tree_depth && next_free == 1 &&
	    ocfs2_rec_clusters(last_el, &last_el->l_recs[i]) == clusters_to_del)
		credits += 1 + tree_depth;

	/* update to the truncate log. */
	credits += OCFS2_TRUNCATE_LOG_UPDATE;

	credits += ocfs2_quota_trans_credits(sb);

	return credits;
}

static inline int ocfs2_jbd2_file_inode(handle_t *handle, struct inode *inode)
{
	return jbd2_journal_file_inode(handle, &OCFS2_I(inode)->ip_jinode);
}

static inline int ocfs2_begin_ordered_truncate(struct inode *inode,
					       loff_t new_size)
{
	return jbd2_journal_begin_ordered_truncate(
				OCFS2_SB(inode->i_sb)->journal->j_journal,
				&OCFS2_I(inode)->ip_jinode,
				new_size);
}

#endif /* OCFS2_JOURNAL_H */
