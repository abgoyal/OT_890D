

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <net/protocol.h>
#include <net/tcp.h>
#include <asm/unaligned.h>

#include <net/ip_vs.h>


#define SERVER_STRING "227 Entering Passive Mode ("
#define CLIENT_STRING "PORT "


static unsigned short ports[IP_VS_APP_MAX_PORTS] = {21, 0};
module_param_array(ports, ushort, NULL, 0);
MODULE_PARM_DESC(ports, "Ports to monitor for FTP control commands");


/*	Dummy variable */
static int ip_vs_ftp_pasv;


static int
ip_vs_ftp_init_conn(struct ip_vs_app *app, struct ip_vs_conn *cp)
{
	return 0;
}


static int
ip_vs_ftp_done_conn(struct ip_vs_app *app, struct ip_vs_conn *cp)
{
	return 0;
}


static int ip_vs_ftp_get_addrport(char *data, char *data_limit,
				  const char *pattern, size_t plen, char term,
				  __be32 *addr, __be16 *port,
				  char **start, char **end)
{
	unsigned char p[6];
	int i = 0;

	if (data_limit - data < plen) {
		/* check if there is partial match */
		if (strnicmp(data, pattern, data_limit - data) == 0)
			return -1;
		else
			return 0;
	}

	if (strnicmp(data, pattern, plen) != 0) {
		return 0;
	}
	*start = data + plen;

	for (data = *start; *data != term; data++) {
		if (data == data_limit)
			return -1;
	}
	*end = data;

	memset(p, 0, sizeof(p));
	for (data = *start; data != *end; data++) {
		if (*data >= '0' && *data <= '9') {
			p[i] = p[i]*10 + *data - '0';
		} else if (*data == ',' && i < 5) {
			i++;
		} else {
			/* unexpected character */
			return -1;
		}
	}

	if (i != 5)
		return -1;

	*addr = get_unaligned((__be32 *)p);
	*port = get_unaligned((__be16 *)(p + 4));
	return 1;
}


static int ip_vs_ftp_out(struct ip_vs_app *app, struct ip_vs_conn *cp,
			 struct sk_buff *skb, int *diff)
{
	struct iphdr *iph;
	struct tcphdr *th;
	char *data, *data_limit;
	char *start, *end;
	union nf_inet_addr from;
	__be16 port;
	struct ip_vs_conn *n_cp;
	char buf[24];		/* xxx.xxx.xxx.xxx,ppp,ppp\000 */
	unsigned buf_len;
	int ret;

#ifdef CONFIG_IP_VS_IPV6
	/* This application helper doesn't work with IPv6 yet,
	 * so turn this into a no-op for IPv6 packets
	 */
	if (cp->af == AF_INET6)
		return 1;
#endif

	*diff = 0;

	/* Only useful for established sessions */
	if (cp->state != IP_VS_TCP_S_ESTABLISHED)
		return 1;

	/* Linear packets are much easier to deal with. */
	if (!skb_make_writable(skb, skb->len))
		return 0;

	if (cp->app_data == &ip_vs_ftp_pasv) {
		iph = ip_hdr(skb);
		th = (struct tcphdr *)&(((char *)iph)[iph->ihl*4]);
		data = (char *)th + (th->doff << 2);
		data_limit = skb_tail_pointer(skb);

		if (ip_vs_ftp_get_addrport(data, data_limit,
					   SERVER_STRING,
					   sizeof(SERVER_STRING)-1, ')',
					   &from.ip, &port,
					   &start, &end) != 1)
			return 1;

		IP_VS_DBG(7, "PASV response (%pI4:%d) -> %pI4:%d detected\n",
			  &from.ip, ntohs(port), &cp->caddr.ip, 0);

		/*
		 * Now update or create an connection entry for it
		 */
		n_cp = ip_vs_conn_out_get(AF_INET, iph->protocol, &from, port,
					  &cp->caddr, 0);
		if (!n_cp) {
			n_cp = ip_vs_conn_new(AF_INET, IPPROTO_TCP,
					      &cp->caddr, 0,
					      &cp->vaddr, port,
					      &from, port,
					      IP_VS_CONN_F_NO_CPORT,
					      cp->dest);
			if (!n_cp)
				return 0;

			/* add its controller */
			ip_vs_control_add(n_cp, cp);
		}

		/*
		 * Replace the old passive address with the new one
		 */
		from.ip = n_cp->vaddr.ip;
		port = n_cp->vport;
		sprintf(buf, "%d,%d,%d,%d,%d,%d", NIPQUAD(from.ip),
			(ntohs(port)>>8)&255, ntohs(port)&255);
		buf_len = strlen(buf);

		/*
		 * Calculate required delta-offset to keep TCP happy
		 */
		*diff = buf_len - (end-start);

		if (*diff == 0) {
			/* simply replace it with new passive address */
			memcpy(start, buf, buf_len);
			ret = 1;
		} else {
			ret = !ip_vs_skb_replace(skb, GFP_ATOMIC, start,
					  end-start, buf, buf_len);
		}

		cp->app_data = NULL;
		ip_vs_tcp_conn_listen(n_cp);
		ip_vs_conn_put(n_cp);
		return ret;
	}
	return 1;
}


static int ip_vs_ftp_in(struct ip_vs_app *app, struct ip_vs_conn *cp,
			struct sk_buff *skb, int *diff)
{
	struct iphdr *iph;
	struct tcphdr *th;
	char *data, *data_start, *data_limit;
	char *start, *end;
	union nf_inet_addr to;
	__be16 port;
	struct ip_vs_conn *n_cp;

#ifdef CONFIG_IP_VS_IPV6
	/* This application helper doesn't work with IPv6 yet,
	 * so turn this into a no-op for IPv6 packets
	 */
	if (cp->af == AF_INET6)
		return 1;
#endif

	/* no diff required for incoming packets */
	*diff = 0;

	/* Only useful for established sessions */
	if (cp->state != IP_VS_TCP_S_ESTABLISHED)
		return 1;

	/* Linear packets are much easier to deal with. */
	if (!skb_make_writable(skb, skb->len))
		return 0;

	/*
	 * Detecting whether it is passive
	 */
	iph = ip_hdr(skb);
	th = (struct tcphdr *)&(((char *)iph)[iph->ihl*4]);

	/* Since there may be OPTIONS in the TCP packet and the HLEN is
	   the length of the header in 32-bit multiples, it is accurate
	   to calculate data address by th+HLEN*4 */
	data = data_start = (char *)th + (th->doff << 2);
	data_limit = skb_tail_pointer(skb);

	while (data <= data_limit - 6) {
		if (strnicmp(data, "PASV\r\n", 6) == 0) {
			/* Passive mode on */
			IP_VS_DBG(7, "got PASV at %td of %td\n",
				  data - data_start,
				  data_limit - data_start);
			cp->app_data = &ip_vs_ftp_pasv;
			return 1;
		}
		data++;
	}

	/*
	 * To support virtual FTP server, the scenerio is as follows:
	 *       FTP client ----> Load Balancer ----> FTP server
	 * First detect the port number in the application data,
	 * then create a new connection entry for the coming data
	 * connection.
	 */
	if (ip_vs_ftp_get_addrport(data_start, data_limit,
				   CLIENT_STRING, sizeof(CLIENT_STRING)-1,
				   '\r', &to.ip, &port,
				   &start, &end) != 1)
		return 1;

	IP_VS_DBG(7, "PORT %pI4:%d detected\n", &to.ip, ntohs(port));

	/* Passive mode off */
	cp->app_data = NULL;

	/*
	 * Now update or create a connection entry for it
	 */
	IP_VS_DBG(7, "protocol %s %pI4:%d %pI4:%d\n",
		  ip_vs_proto_name(iph->protocol),
		  &to.ip, ntohs(port), &cp->vaddr.ip, 0);

	n_cp = ip_vs_conn_in_get(AF_INET, iph->protocol,
				 &to, port,
				 &cp->vaddr, htons(ntohs(cp->vport)-1));
	if (!n_cp) {
		n_cp = ip_vs_conn_new(AF_INET, IPPROTO_TCP,
				      &to, port,
				      &cp->vaddr, htons(ntohs(cp->vport)-1),
				      &cp->daddr, htons(ntohs(cp->dport)-1),
				      0,
				      cp->dest);
		if (!n_cp)
			return 0;

		/* add its controller */
		ip_vs_control_add(n_cp, cp);
	}

	/*
	 *	Move tunnel to listen state
	 */
	ip_vs_tcp_conn_listen(n_cp);
	ip_vs_conn_put(n_cp);

	return 1;
}


static struct ip_vs_app ip_vs_ftp = {
	.name =		"ftp",
	.type =		IP_VS_APP_TYPE_FTP,
	.protocol =	IPPROTO_TCP,
	.module =	THIS_MODULE,
	.incs_list =	LIST_HEAD_INIT(ip_vs_ftp.incs_list),
	.init_conn =	ip_vs_ftp_init_conn,
	.done_conn =	ip_vs_ftp_done_conn,
	.bind_conn =	NULL,
	.unbind_conn =	NULL,
	.pkt_out =	ip_vs_ftp_out,
	.pkt_in =	ip_vs_ftp_in,
};


static int __init ip_vs_ftp_init(void)
{
	int i, ret;
	struct ip_vs_app *app = &ip_vs_ftp;

	ret = register_ip_vs_app(app);
	if (ret)
		return ret;

	for (i=0; i<IP_VS_APP_MAX_PORTS; i++) {
		if (!ports[i])
			continue;
		ret = register_ip_vs_app_inc(app, app->protocol, ports[i]);
		if (ret)
			break;
		IP_VS_INFO("%s: loaded support on port[%d] = %d\n",
			   app->name, i, ports[i]);
	}

	if (ret)
		unregister_ip_vs_app(app);

	return ret;
}


static void __exit ip_vs_ftp_exit(void)
{
	unregister_ip_vs_app(&ip_vs_ftp);
}


module_init(ip_vs_ftp_init);
module_exit(ip_vs_ftp_exit);
MODULE_LICENSE("GPL");
