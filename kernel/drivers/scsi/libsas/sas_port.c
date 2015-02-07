

#include "sas_internal.h"

#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_sas.h>
#include "../scsi_sas_internal.h"

static void sas_form_port(struct asd_sas_phy *phy)
{
	int i;
	struct sas_ha_struct *sas_ha = phy->ha;
	struct asd_sas_port *port = phy->port;
	struct sas_internal *si =
		to_sas_internal(sas_ha->core.shost->transportt);
	unsigned long flags;

	if (port) {
		if (memcmp(port->attached_sas_addr, phy->attached_sas_addr,
			   SAS_ADDR_SIZE) != 0)
			sas_deform_port(phy);
		else {
			SAS_DPRINTK("%s: phy%d belongs to port%d already(%d)!\n",
				    __func__, phy->id, phy->port->id,
				    phy->port->num_phys);
			return;
		}
	}

	/* find a port */
	spin_lock_irqsave(&sas_ha->phy_port_lock, flags);
	for (i = 0; i < sas_ha->num_phys; i++) {
		port = sas_ha->sas_port[i];
		spin_lock(&port->phy_list_lock);
		if (*(u64 *) port->sas_addr &&
		    memcmp(port->attached_sas_addr,
			   phy->attached_sas_addr, SAS_ADDR_SIZE) == 0 &&
		    port->num_phys > 0) {
			/* wide port */
			SAS_DPRINTK("phy%d matched wide port%d\n", phy->id,
				    port->id);
			break;
		} else if (*(u64 *) port->sas_addr == 0 && port->num_phys==0) {
			memcpy(port->sas_addr, phy->sas_addr, SAS_ADDR_SIZE);
			break;
		}
		spin_unlock(&port->phy_list_lock);
	}

	if (i >= sas_ha->num_phys) {
		printk(KERN_NOTICE "%s: couldn't find a free port, bug?\n",
		       __func__);
		spin_unlock_irqrestore(&sas_ha->phy_port_lock, flags);
		return;
	}

	/* add the phy to the port */
	list_add_tail(&phy->port_phy_el, &port->phy_list);
	phy->port = port;
	port->num_phys++;
	port->phy_mask |= (1U << phy->id);

	if (!port->phy)
		port->phy = phy->phy;

	if (*(u64 *)port->attached_sas_addr == 0) {
		port->class = phy->class;
		memcpy(port->attached_sas_addr, phy->attached_sas_addr,
		       SAS_ADDR_SIZE);
		port->iproto = phy->iproto;
		port->tproto = phy->tproto;
		port->oob_mode = phy->oob_mode;
		port->linkrate = phy->linkrate;
	} else
		port->linkrate = max(port->linkrate, phy->linkrate);
	spin_unlock(&port->phy_list_lock);
	spin_unlock_irqrestore(&sas_ha->phy_port_lock, flags);

	if (!port->port) {
		port->port = sas_port_alloc(phy->phy->dev.parent, port->id);
		BUG_ON(!port->port);
		sas_port_add(port->port);
	}
	sas_port_add_phy(port->port, phy->phy);

	SAS_DPRINTK("%s added to %s, phy_mask:0x%x (%16llx)\n",
		    dev_name(&phy->phy->dev), dev_name(&port->port->dev),
		    port->phy_mask,
		    SAS_ADDR(port->attached_sas_addr));

	if (port->port_dev)
		port->port_dev->pathways = port->num_phys;

	/* Tell the LLDD about this port formation. */
	if (si->dft->lldd_port_formed)
		si->dft->lldd_port_formed(phy);

	sas_discover_event(phy->port, DISCE_DISCOVER_DOMAIN);
}

void sas_deform_port(struct asd_sas_phy *phy)
{
	struct sas_ha_struct *sas_ha = phy->ha;
	struct asd_sas_port *port = phy->port;
	struct sas_internal *si =
		to_sas_internal(sas_ha->core.shost->transportt);
	unsigned long flags;

	if (!port)
		return;		  /* done by a phy event */

	if (port->port_dev)
		port->port_dev->pathways--;

	if (port->num_phys == 1) {
		sas_unregister_domain_devices(port);
		sas_port_delete(port->port);
		port->port = NULL;
	} else
		sas_port_delete_phy(port->port, phy->phy);


	if (si->dft->lldd_port_deformed)
		si->dft->lldd_port_deformed(phy);

	spin_lock_irqsave(&sas_ha->phy_port_lock, flags);
	spin_lock(&port->phy_list_lock);

	list_del_init(&phy->port_phy_el);
	phy->port = NULL;
	port->num_phys--;
	port->phy_mask &= ~(1U << phy->id);

	if (port->num_phys == 0) {
		INIT_LIST_HEAD(&port->phy_list);
		memset(port->sas_addr, 0, SAS_ADDR_SIZE);
		memset(port->attached_sas_addr, 0, SAS_ADDR_SIZE);
		port->class = 0;
		port->iproto = 0;
		port->tproto = 0;
		port->oob_mode = 0;
		port->phy_mask = 0;
	}
	spin_unlock(&port->phy_list_lock);
	spin_unlock_irqrestore(&sas_ha->phy_port_lock, flags);

	return;
}

/* ---------- SAS port events ---------- */

void sas_porte_bytes_dmaed(struct work_struct *work)
{
	struct asd_sas_event *ev =
		container_of(work, struct asd_sas_event, work);
	struct asd_sas_phy *phy = ev->phy;

	sas_begin_event(PORTE_BYTES_DMAED, &phy->ha->event_lock,
			&phy->port_events_pending);

	sas_form_port(phy);
}

void sas_porte_broadcast_rcvd(struct work_struct *work)
{
	struct asd_sas_event *ev =
		container_of(work, struct asd_sas_event, work);
	struct asd_sas_phy *phy = ev->phy;
	unsigned long flags;
	u32 prim;

	sas_begin_event(PORTE_BROADCAST_RCVD, &phy->ha->event_lock,
			&phy->port_events_pending);

	spin_lock_irqsave(&phy->sas_prim_lock, flags);
	prim = phy->sas_prim;
	spin_unlock_irqrestore(&phy->sas_prim_lock, flags);

	SAS_DPRINTK("broadcast received: %d\n", prim);
	sas_discover_event(phy->port, DISCE_REVALIDATE_DOMAIN);
}

void sas_porte_link_reset_err(struct work_struct *work)
{
	struct asd_sas_event *ev =
		container_of(work, struct asd_sas_event, work);
	struct asd_sas_phy *phy = ev->phy;

	sas_begin_event(PORTE_LINK_RESET_ERR, &phy->ha->event_lock,
			&phy->port_events_pending);

	sas_deform_port(phy);
}

void sas_porte_timer_event(struct work_struct *work)
{
	struct asd_sas_event *ev =
		container_of(work, struct asd_sas_event, work);
	struct asd_sas_phy *phy = ev->phy;

	sas_begin_event(PORTE_TIMER_EVENT, &phy->ha->event_lock,
			&phy->port_events_pending);

	sas_deform_port(phy);
}

void sas_porte_hard_reset(struct work_struct *work)
{
	struct asd_sas_event *ev =
		container_of(work, struct asd_sas_event, work);
	struct asd_sas_phy *phy = ev->phy;

	sas_begin_event(PORTE_HARD_RESET, &phy->ha->event_lock,
			&phy->port_events_pending);

	sas_deform_port(phy);
}

/* ---------- SAS port registration ---------- */

static void sas_init_port(struct asd_sas_port *port,
			  struct sas_ha_struct *sas_ha, int i)
{
	memset(port, 0, sizeof(*port));
	port->id = i;
	INIT_LIST_HEAD(&port->dev_list);
	spin_lock_init(&port->phy_list_lock);
	INIT_LIST_HEAD(&port->phy_list);
	port->ha = sas_ha;

	spin_lock_init(&port->dev_list_lock);
}

int sas_register_ports(struct sas_ha_struct *sas_ha)
{
	int i;

	/* initialize the ports and discovery */
	for (i = 0; i < sas_ha->num_phys; i++) {
		struct asd_sas_port *port = sas_ha->sas_port[i];

		sas_init_port(port, sas_ha, i);
		sas_init_disc(&port->disc, port);
	}
	return 0;
}

void sas_unregister_ports(struct sas_ha_struct *sas_ha)
{
	int i;

	for (i = 0; i < sas_ha->num_phys; i++)
		if (sas_ha->sas_phy[i]->port)
			sas_deform_port(sas_ha->sas_phy[i]);

}