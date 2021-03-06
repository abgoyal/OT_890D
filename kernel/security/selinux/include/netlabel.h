


#ifndef _SELINUX_NETLABEL_H_
#define _SELINUX_NETLABEL_H_

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <net/sock.h>

#include "avc.h"
#include "objsec.h"

#ifdef CONFIG_NETLABEL
void selinux_netlbl_cache_invalidate(void);

void selinux_netlbl_err(struct sk_buff *skb, int error, int gateway);

void selinux_netlbl_sk_security_free(struct sk_security_struct *ssec);
void selinux_netlbl_sk_security_reset(struct sk_security_struct *ssec,
				      int family);

int selinux_netlbl_skbuff_getsid(struct sk_buff *skb,
				 u16 family,
				 u32 *type,
				 u32 *sid);
int selinux_netlbl_skbuff_setsid(struct sk_buff *skb,
				 u16 family,
				 u32 sid);

void selinux_netlbl_inet_conn_established(struct sock *sk, u16 family);
int selinux_netlbl_socket_post_create(struct socket *sock);
int selinux_netlbl_inode_permission(struct inode *inode, int mask);
int selinux_netlbl_sock_rcv_skb(struct sk_security_struct *sksec,
				struct sk_buff *skb,
				u16 family,
				struct avc_audit_data *ad);
int selinux_netlbl_socket_setsockopt(struct socket *sock,
				     int level,
				     int optname);
int selinux_netlbl_socket_connect(struct sock *sk, struct sockaddr *addr);

#else
static inline void selinux_netlbl_cache_invalidate(void)
{
	return;
}

static inline void selinux_netlbl_err(struct sk_buff *skb,
				      int error,
				      int gateway)
{
	return;
}

static inline void selinux_netlbl_sk_security_free(
					       struct sk_security_struct *ssec)
{
	return;
}

static inline void selinux_netlbl_sk_security_reset(
					       struct sk_security_struct *ssec,
					       int family)
{
	return;
}

static inline int selinux_netlbl_skbuff_getsid(struct sk_buff *skb,
					       u16 family,
					       u32 *type,
					       u32 *sid)
{
	*type = NETLBL_NLTYPE_NONE;
	*sid = SECSID_NULL;
	return 0;
}
static inline int selinux_netlbl_skbuff_setsid(struct sk_buff *skb,
					       u16 family,
					       u32 sid)
{
	return 0;
}

static inline int selinux_netlbl_conn_setsid(struct sock *sk,
					     struct sockaddr *addr)
{
	return 0;
}

static inline void selinux_netlbl_inet_conn_established(struct sock *sk,
							u16 family)
{
	return;
}
static inline int selinux_netlbl_socket_post_create(struct socket *sock)
{
	return 0;
}
static inline int selinux_netlbl_inode_permission(struct inode *inode,
						  int mask)
{
	return 0;
}
static inline int selinux_netlbl_sock_rcv_skb(struct sk_security_struct *sksec,
					      struct sk_buff *skb,
					      u16 family,
					      struct avc_audit_data *ad)
{
	return 0;
}
static inline int selinux_netlbl_socket_setsockopt(struct socket *sock,
						   int level,
						   int optname)
{
	return 0;
}
static inline int selinux_netlbl_socket_connect(struct sock *sk,
						struct sockaddr *addr)
{
	return 0;
}
#endif /* CONFIG_NETLABEL */

#endif
