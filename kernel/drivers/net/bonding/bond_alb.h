

#ifndef __BOND_ALB_H__
#define __BOND_ALB_H__

#include <linux/if_ether.h>

struct bonding;
struct slave;

#define BOND_ALB_INFO(bond)   ((bond)->alb_info)
#define SLAVE_TLB_INFO(slave) ((slave)->tlb_info)

struct tlb_client_info {
	struct slave *tx_slave;	/* A pointer to slave used for transmiting
				 * packets to a Client that the Hash function
				 * gave this entry index.
				 */
	u32 tx_bytes;		/* Each Client acumulates the BytesTx that
				 * were tranmitted to it, and after each
				 * CallBack the LoadHistory is devided
				 * by the balance interval
				 */
	u32 load_history;	/* This field contains the amount of Bytes
				 * that were transmitted to this client by
				 * the server on the previous balance
				 * interval in Bps.
				 */
	u32 next;		/* The next Hash table entry index, assigned
				 * to use the same adapter for transmit.
				 */
	u32 prev;		/* The previous Hash table entry index,
				 * assigned to use the same
				 */
};

struct rlb_client_info {
	__be32 ip_src;		/* the server IP address */
	__be32 ip_dst;		/* the client IP address */
	u8  mac_dst[ETH_ALEN];	/* the client MAC address */
	u32 next;		/* The next Hash table entry index */
	u32 prev;		/* The previous Hash table entry index */
	u8  assigned;		/* checking whether this entry is assigned */
	u8  ntt;		/* flag - need to transmit client info */
	struct slave *slave;	/* the slave assigned to this client */
	u8 tag;			/* flag - need to tag skb */
	unsigned short vlan_id;	/* VLAN tag associated with IP address */
};

struct tlb_slave_info {
	u32 head;	/* Index to the head of the bi-directional clients
			 * hash table entries list. The entries in the list
			 * are the entries that were assigned to use this
			 * slave for transmit.
			 */
	u32 load;	/* Each slave sums the loadHistory of all clients
			 * assigned to it
			 */
};

struct alb_bond_info {
	struct timer_list	alb_timer;
	struct tlb_client_info	*tx_hashtbl; /* Dynamically allocated */
	spinlock_t		tx_hashtbl_lock;
	u32			unbalanced_load;
	int			tx_rebalance_counter;
	int			lp_counter;
	/* -------- rlb parameters -------- */
	int rlb_enabled;
	struct packet_type	rlb_pkt_type;
	struct rlb_client_info	*rx_hashtbl;	/* Receive hash table */
	spinlock_t		rx_hashtbl_lock;
	u32			rx_hashtbl_head;
	u8			rx_ntt;	/* flag - need to transmit
					 * to all rx clients
					 */
	struct slave		*next_rx_slave;/* next slave to be assigned
						* to a new rx client for
						*/
	u32			rlb_interval_counter;
	u8			primary_is_promisc;	   /* boolean */
	u32			rlb_promisc_timeout_counter;/* counts primary
							     * promiscuity time
							     */
	u32			rlb_update_delay_counter;
	u32			rlb_update_retry_counter;/* counter of retries
							  * of client update
							  */
	u8			rlb_rebalance;	/* flag - indicates that the
						 * rx traffic should be
						 * rebalanced
						 */
	struct vlan_entry	*current_alb_vlan;
};

int bond_alb_initialize(struct bonding *bond, int rlb_enabled);
void bond_alb_deinitialize(struct bonding *bond);
int bond_alb_init_slave(struct bonding *bond, struct slave *slave);
void bond_alb_deinit_slave(struct bonding *bond, struct slave *slave);
void bond_alb_handle_link_change(struct bonding *bond, struct slave *slave, char link);
void bond_alb_handle_active_change(struct bonding *bond, struct slave *new_slave);
int bond_alb_xmit(struct sk_buff *skb, struct net_device *bond_dev);
void bond_alb_monitor(struct work_struct *);
int bond_alb_set_mac_address(struct net_device *bond_dev, void *addr);
void bond_alb_clear_vlan(struct bonding *bond, unsigned short vlan_id);
#endif /* __BOND_ALB_H__ */

