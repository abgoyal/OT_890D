

#ifndef __QUOTA_DOT_H__
#define __QUOTA_DOT_H__

struct gfs2_inode;
struct gfs2_sbd;

#define NO_QUOTA_CHANGE ((u32)-1)

extern int gfs2_quota_hold(struct gfs2_inode *ip, u32 uid, u32 gid);
extern void gfs2_quota_unhold(struct gfs2_inode *ip);

extern int gfs2_quota_lock(struct gfs2_inode *ip, u32 uid, u32 gid);
extern void gfs2_quota_unlock(struct gfs2_inode *ip);

extern int gfs2_quota_check(struct gfs2_inode *ip, u32 uid, u32 gid);
extern void gfs2_quota_change(struct gfs2_inode *ip, s64 change,
			      u32 uid, u32 gid);

extern int gfs2_quota_sync(struct gfs2_sbd *sdp);
extern int gfs2_quota_refresh(struct gfs2_sbd *sdp, int user, u32 id);

extern int gfs2_quota_init(struct gfs2_sbd *sdp);
extern void gfs2_quota_cleanup(struct gfs2_sbd *sdp);
extern int gfs2_quotad(void *data);

static inline int gfs2_quota_lock_check(struct gfs2_inode *ip)
{
	struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);
	int ret;
	if (sdp->sd_args.ar_quota == GFS2_QUOTA_OFF)
		return 0;
	ret = gfs2_quota_lock(ip, NO_QUOTA_CHANGE, NO_QUOTA_CHANGE);
	if (ret)
		return ret;
	if (sdp->sd_args.ar_quota != GFS2_QUOTA_ON)
		return 0;
	ret = gfs2_quota_check(ip, ip->i_inode.i_uid, ip->i_inode.i_gid);
	if (ret)
		gfs2_quota_unlock(ip);
	return ret;
}

#endif /* __QUOTA_DOT_H__ */
