


#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>

struct inet6_protocol *inet6_protos[MAX_INET_PROTOS];
static DEFINE_SPINLOCK(inet6_proto_lock);


int inet6_add_protocol(struct inet6_protocol *prot, unsigned char protocol)
{
	int ret, hash = protocol & (MAX_INET_PROTOS - 1);

	spin_lock_bh(&inet6_proto_lock);

	if (inet6_protos[hash]) {
		ret = -1;
	} else {
		inet6_protos[hash] = prot;
		ret = 0;
	}

	spin_unlock_bh(&inet6_proto_lock);

	return ret;
}

EXPORT_SYMBOL(inet6_add_protocol);


int inet6_del_protocol(struct inet6_protocol *prot, unsigned char protocol)
{
	int ret, hash = protocol & (MAX_INET_PROTOS - 1);

	spin_lock_bh(&inet6_proto_lock);

	if (inet6_protos[hash] != prot) {
		ret = -1;
	} else {
		inet6_protos[hash] = NULL;
		ret = 0;
	}

	spin_unlock_bh(&inet6_proto_lock);

	synchronize_net();

	return ret;
}

EXPORT_SYMBOL(inet6_del_protocol);
