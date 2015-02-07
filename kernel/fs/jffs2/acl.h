

struct jffs2_acl_entry {
	jint16_t	e_tag;
	jint16_t	e_perm;
	jint32_t	e_id;
};

struct jffs2_acl_entry_short {
	jint16_t	e_tag;
	jint16_t	e_perm;
};

struct jffs2_acl_header {
	jint32_t	a_version;
};

#ifdef CONFIG_JFFS2_FS_POSIX_ACL

#define JFFS2_ACL_NOT_CACHED ((void *)-1)

extern int jffs2_permission(struct inode *, int);
extern int jffs2_acl_chmod(struct inode *);
extern int jffs2_init_acl_pre(struct inode *, struct inode *, int *);
extern int jffs2_init_acl_post(struct inode *);
extern void jffs2_clear_acl(struct jffs2_inode_info *);

extern struct xattr_handler jffs2_acl_access_xattr_handler;
extern struct xattr_handler jffs2_acl_default_xattr_handler;

#else

#define jffs2_permission			(NULL)
#define jffs2_acl_chmod(inode)			(0)
#define jffs2_init_acl_pre(dir_i,inode,mode)	(0)
#define jffs2_init_acl_post(inode)		(0)
#define jffs2_clear_acl(f)

#endif	/* CONFIG_JFFS2_FS_POSIX_ACL */