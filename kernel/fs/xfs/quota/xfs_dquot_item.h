
#ifndef __XFS_DQUOT_ITEM_H__
#define __XFS_DQUOT_ITEM_H__

struct xfs_dquot;
struct xfs_trans;
struct xfs_mount;
struct xfs_qoff_logitem;

typedef struct xfs_dq_logitem {
	xfs_log_item_t		 qli_item;	   /* common portion */
	struct xfs_dquot	*qli_dquot;	   /* dquot ptr */
	xfs_lsn_t		 qli_flush_lsn;	   /* lsn at last flush */
	unsigned short		 qli_pushbuf_flag; /* 1 bit used in push_ail */
#ifdef DEBUG
	uint64_t		 qli_push_owner;
#endif
	xfs_dq_logformat_t	 qli_format;	   /* logged structure */
} xfs_dq_logitem_t;

typedef struct xfs_qoff_logitem {
	xfs_log_item_t		 qql_item;	/* common portion */
	struct xfs_qoff_logitem *qql_start_lip; /* qoff-start logitem, if any */
	xfs_qoff_logformat_t	 qql_format;	/* logged structure */
} xfs_qoff_logitem_t;


extern void		   xfs_qm_dquot_logitem_init(struct xfs_dquot *);
extern xfs_qoff_logitem_t *xfs_qm_qoff_logitem_init(struct xfs_mount *,
					struct xfs_qoff_logitem *, uint);
extern xfs_qoff_logitem_t *xfs_trans_get_qoff_item(struct xfs_trans *,
					struct xfs_qoff_logitem *, uint);
extern void		   xfs_trans_log_quotaoff_item(struct xfs_trans *,
					struct xfs_qoff_logitem *);

#endif	/* __XFS_DQUOT_ITEM_H__ */
