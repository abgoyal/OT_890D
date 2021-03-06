

#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/delay.h>
#include <linux/pci.h>

#include "e1000.h"

enum e1000_mng_mode {
	e1000_mng_mode_none = 0,
	e1000_mng_mode_asf,
	e1000_mng_mode_pt,
	e1000_mng_mode_ipmi,
	e1000_mng_mode_host_if_only
};

#define E1000_FACTPS_MNGCG		0x20000000

/* Intel(R) Active Management Technology signature */
#define E1000_IAMT_SIGNATURE		0x544D4149

s32 e1000e_get_bus_info_pcie(struct e1000_hw *hw)
{
	struct e1000_bus_info *bus = &hw->bus;
	struct e1000_adapter *adapter = hw->adapter;
	u32 status;
	u16 pcie_link_status, pci_header_type, cap_offset;

	cap_offset = pci_find_capability(adapter->pdev, PCI_CAP_ID_EXP);
	if (!cap_offset) {
		bus->width = e1000_bus_width_unknown;
	} else {
		pci_read_config_word(adapter->pdev,
				     cap_offset + PCIE_LINK_STATUS,
				     &pcie_link_status);
		bus->width = (enum e1000_bus_width)((pcie_link_status &
						     PCIE_LINK_WIDTH_MASK) >>
						    PCIE_LINK_WIDTH_SHIFT);
	}

	pci_read_config_word(adapter->pdev, PCI_HEADER_TYPE_REGISTER,
			     &pci_header_type);
	if (pci_header_type & PCI_HEADER_TYPE_MULTIFUNC) {
		status = er32(STATUS);
		bus->func = (status & E1000_STATUS_FUNC_MASK)
			    >> E1000_STATUS_FUNC_SHIFT;
	} else {
		bus->func = 0;
	}

	return 0;
}

void e1000e_write_vfta(struct e1000_hw *hw, u32 offset, u32 value)
{
	E1000_WRITE_REG_ARRAY(hw, E1000_VFTA, offset, value);
	e1e_flush();
}

void e1000e_init_rx_addrs(struct e1000_hw *hw, u16 rar_count)
{
	u32 i;

	/* Setup the receive address */
	hw_dbg(hw, "Programming MAC Address into RAR[0]\n");

	e1000e_rar_set(hw, hw->mac.addr, 0);

	/* Zero out the other (rar_entry_count - 1) receive addresses */
	hw_dbg(hw, "Clearing RAR[1-%u]\n", rar_count-1);
	for (i = 1; i < rar_count; i++) {
		E1000_WRITE_REG_ARRAY(hw, E1000_RA, (i << 1), 0);
		e1e_flush();
		E1000_WRITE_REG_ARRAY(hw, E1000_RA, ((i << 1) + 1), 0);
		e1e_flush();
	}
}

void e1000e_rar_set(struct e1000_hw *hw, u8 *addr, u32 index)
{
	u32 rar_low, rar_high;

	/*
	 * HW expects these in little endian so we reverse the byte order
	 * from network order (big endian) to little endian
	 */
	rar_low = ((u32) addr[0] |
		   ((u32) addr[1] << 8) |
		    ((u32) addr[2] << 16) | ((u32) addr[3] << 24));

	rar_high = ((u32) addr[4] | ((u32) addr[5] << 8));

	rar_high |= E1000_RAH_AV;

	E1000_WRITE_REG_ARRAY(hw, E1000_RA, (index << 1), rar_low);
	E1000_WRITE_REG_ARRAY(hw, E1000_RA, ((index << 1) + 1), rar_high);
}

static void e1000_mta_set(struct e1000_hw *hw, u32 hash_value)
{
	u32 hash_bit, hash_reg, mta;

	/*
	 * The MTA is a register array of 32-bit registers. It is
	 * treated like an array of (32*mta_reg_count) bits.  We want to
	 * set bit BitArray[hash_value]. So we figure out what register
	 * the bit is in, read it, OR in the new bit, then write
	 * back the new value.  The (hw->mac.mta_reg_count - 1) serves as a
	 * mask to bits 31:5 of the hash value which gives us the
	 * register we're modifying.  The hash bit within that register
	 * is determined by the lower 5 bits of the hash value.
	 */
	hash_reg = (hash_value >> 5) & (hw->mac.mta_reg_count - 1);
	hash_bit = hash_value & 0x1F;

	mta = E1000_READ_REG_ARRAY(hw, E1000_MTA, hash_reg);

	mta |= (1 << hash_bit);

	E1000_WRITE_REG_ARRAY(hw, E1000_MTA, hash_reg, mta);
	e1e_flush();
}

static u32 e1000_hash_mc_addr(struct e1000_hw *hw, u8 *mc_addr)
{
	u32 hash_value, hash_mask;
	u8 bit_shift = 0;

	/* Register count multiplied by bits per register */
	hash_mask = (hw->mac.mta_reg_count * 32) - 1;

	/*
	 * For a mc_filter_type of 0, bit_shift is the number of left-shifts
	 * where 0xFF would still fall within the hash mask.
	 */
	while (hash_mask >> bit_shift != 0xFF)
		bit_shift++;

	/*
	 * The portion of the address that is used for the hash table
	 * is determined by the mc_filter_type setting.
	 * The algorithm is such that there is a total of 8 bits of shifting.
	 * The bit_shift for a mc_filter_type of 0 represents the number of
	 * left-shifts where the MSB of mc_addr[5] would still fall within
	 * the hash_mask.  Case 0 does this exactly.  Since there are a total
	 * of 8 bits of shifting, then mc_addr[4] will shift right the
	 * remaining number of bits. Thus 8 - bit_shift.  The rest of the
	 * cases are a variation of this algorithm...essentially raising the
	 * number of bits to shift mc_addr[5] left, while still keeping the
	 * 8-bit shifting total.
	 *
	 * For example, given the following Destination MAC Address and an
	 * mta register count of 128 (thus a 4096-bit vector and 0xFFF mask),
	 * we can see that the bit_shift for case 0 is 4.  These are the hash
	 * values resulting from each mc_filter_type...
	 * [0] [1] [2] [3] [4] [5]
	 * 01  AA  00  12  34  56
	 * LSB		 MSB
	 *
	 * case 0: hash_value = ((0x34 >> 4) | (0x56 << 4)) & 0xFFF = 0x563
	 * case 1: hash_value = ((0x34 >> 3) | (0x56 << 5)) & 0xFFF = 0xAC6
	 * case 2: hash_value = ((0x34 >> 2) | (0x56 << 6)) & 0xFFF = 0x163
	 * case 3: hash_value = ((0x34 >> 0) | (0x56 << 8)) & 0xFFF = 0x634
	 */
	switch (hw->mac.mc_filter_type) {
	default:
	case 0:
		break;
	case 1:
		bit_shift += 1;
		break;
	case 2:
		bit_shift += 2;
		break;
	case 3:
		bit_shift += 4;
		break;
	}

	hash_value = hash_mask & (((mc_addr[4] >> (8 - bit_shift)) |
				  (((u16) mc_addr[5]) << bit_shift)));

	return hash_value;
}

void e1000e_update_mc_addr_list_generic(struct e1000_hw *hw,
					u8 *mc_addr_list, u32 mc_addr_count,
					u32 rar_used_count, u32 rar_count)
{
	u32 hash_value;
	u32 i;

	/*
	 * Load the first set of multicast addresses into the exact
	 * filters (RAR).  If there are not enough to fill the RAR
	 * array, clear the filters.
	 */
	for (i = rar_used_count; i < rar_count; i++) {
		if (mc_addr_count) {
			e1000e_rar_set(hw, mc_addr_list, i);
			mc_addr_count--;
			mc_addr_list += ETH_ALEN;
		} else {
			E1000_WRITE_REG_ARRAY(hw, E1000_RA, i << 1, 0);
			e1e_flush();
			E1000_WRITE_REG_ARRAY(hw, E1000_RA, (i << 1) + 1, 0);
			e1e_flush();
		}
	}

	/* Clear the old settings from the MTA */
	hw_dbg(hw, "Clearing MTA\n");
	for (i = 0; i < hw->mac.mta_reg_count; i++) {
		E1000_WRITE_REG_ARRAY(hw, E1000_MTA, i, 0);
		e1e_flush();
	}

	/* Load any remaining multicast addresses into the hash table. */
	for (; mc_addr_count > 0; mc_addr_count--) {
		hash_value = e1000_hash_mc_addr(hw, mc_addr_list);
		hw_dbg(hw, "Hash value = 0x%03X\n", hash_value);
		e1000_mta_set(hw, hash_value);
		mc_addr_list += ETH_ALEN;
	}
}

void e1000e_clear_hw_cntrs_base(struct e1000_hw *hw)
{
	u32 temp;

	temp = er32(CRCERRS);
	temp = er32(SYMERRS);
	temp = er32(MPC);
	temp = er32(SCC);
	temp = er32(ECOL);
	temp = er32(MCC);
	temp = er32(LATECOL);
	temp = er32(COLC);
	temp = er32(DC);
	temp = er32(SEC);
	temp = er32(RLEC);
	temp = er32(XONRXC);
	temp = er32(XONTXC);
	temp = er32(XOFFRXC);
	temp = er32(XOFFTXC);
	temp = er32(FCRUC);
	temp = er32(GPRC);
	temp = er32(BPRC);
	temp = er32(MPRC);
	temp = er32(GPTC);
	temp = er32(GORCL);
	temp = er32(GORCH);
	temp = er32(GOTCL);
	temp = er32(GOTCH);
	temp = er32(RNBC);
	temp = er32(RUC);
	temp = er32(RFC);
	temp = er32(ROC);
	temp = er32(RJC);
	temp = er32(TORL);
	temp = er32(TORH);
	temp = er32(TOTL);
	temp = er32(TOTH);
	temp = er32(TPR);
	temp = er32(TPT);
	temp = er32(MPTC);
	temp = er32(BPTC);
}

s32 e1000e_check_for_copper_link(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	bool link;

	/*
	 * We only want to go out to the PHY registers to see if Auto-Neg
	 * has completed and/or if our link status has changed.  The
	 * get_link_status flag is set upon receiving a Link Status
	 * Change or Rx Sequence Error interrupt.
	 */
	if (!mac->get_link_status)
		return 0;

	/*
	 * First we want to see if the MII Status Register reports
	 * link.  If so, then we want to get the current speed/duplex
	 * of the PHY.
	 */
	ret_val = e1000e_phy_has_link_generic(hw, 1, 0, &link);
	if (ret_val)
		return ret_val;

	if (!link)
		return ret_val; /* No link detected */

	mac->get_link_status = 0;

	/*
	 * Check if there was DownShift, must be checked
	 * immediately after link-up
	 */
	e1000e_check_downshift(hw);

	/*
	 * If we are forcing speed/duplex, then we simply return since
	 * we have already determined whether we have link or not.
	 */
	if (!mac->autoneg) {
		ret_val = -E1000_ERR_CONFIG;
		return ret_val;
	}

	/*
	 * Auto-Neg is enabled.  Auto Speed Detection takes care
	 * of MAC speed/duplex configuration.  So we only need to
	 * configure Collision Distance in the MAC.
	 */
	e1000e_config_collision_dist(hw);

	/*
	 * Configure Flow Control now that Auto-Neg has completed.
	 * First, we need to restore the desired flow control
	 * settings because we may have had to re-autoneg with a
	 * different link partner.
	 */
	ret_val = e1000e_config_fc_after_link_up(hw);
	if (ret_val) {
		hw_dbg(hw, "Error configuring flow control\n");
	}

	return ret_val;
}

s32 e1000e_check_for_fiber_link(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 rxcw;
	u32 ctrl;
	u32 status;
	s32 ret_val;

	ctrl = er32(CTRL);
	status = er32(STATUS);
	rxcw = er32(RXCW);

	/*
	 * If we don't have link (auto-negotiation failed or link partner
	 * cannot auto-negotiate), the cable is plugged in (we have signal),
	 * and our link partner is not trying to auto-negotiate with us (we
	 * are receiving idles or data), we need to force link up. We also
	 * need to give auto-negotiation time to complete, in case the cable
	 * was just plugged in. The autoneg_failed flag does this.
	 */
	/* (ctrl & E1000_CTRL_SWDPIN1) == 1 == have signal */
	if ((ctrl & E1000_CTRL_SWDPIN1) && (!(status & E1000_STATUS_LU)) &&
	    (!(rxcw & E1000_RXCW_C))) {
		if (mac->autoneg_failed == 0) {
			mac->autoneg_failed = 1;
			return 0;
		}
		hw_dbg(hw, "NOT RXing /C/, disable AutoNeg and force link.\n");

		/* Disable auto-negotiation in the TXCW register */
		ew32(TXCW, (mac->txcw & ~E1000_TXCW_ANE));

		/* Force link-up and also force full-duplex. */
		ctrl = er32(CTRL);
		ctrl |= (E1000_CTRL_SLU | E1000_CTRL_FD);
		ew32(CTRL, ctrl);

		/* Configure Flow Control after forcing link up. */
		ret_val = e1000e_config_fc_after_link_up(hw);
		if (ret_val) {
			hw_dbg(hw, "Error configuring flow control\n");
			return ret_val;
		}
	} else if ((ctrl & E1000_CTRL_SLU) && (rxcw & E1000_RXCW_C)) {
		/*
		 * If we are forcing link and we are receiving /C/ ordered
		 * sets, re-enable auto-negotiation in the TXCW register
		 * and disable forced link in the Device Control register
		 * in an attempt to auto-negotiate with our link partner.
		 */
		hw_dbg(hw, "RXing /C/, enable AutoNeg and stop forcing link.\n");
		ew32(TXCW, mac->txcw);
		ew32(CTRL, (ctrl & ~E1000_CTRL_SLU));

		mac->serdes_has_link = 1;
	}

	return 0;
}

s32 e1000e_check_for_serdes_link(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 rxcw;
	u32 ctrl;
	u32 status;
	s32 ret_val;

	ctrl = er32(CTRL);
	status = er32(STATUS);
	rxcw = er32(RXCW);

	/*
	 * If we don't have link (auto-negotiation failed or link partner
	 * cannot auto-negotiate), and our link partner is not trying to
	 * auto-negotiate with us (we are receiving idles or data),
	 * we need to force link up. We also need to give auto-negotiation
	 * time to complete.
	 */
	/* (ctrl & E1000_CTRL_SWDPIN1) == 1 == have signal */
	if ((!(status & E1000_STATUS_LU)) && (!(rxcw & E1000_RXCW_C))) {
		if (mac->autoneg_failed == 0) {
			mac->autoneg_failed = 1;
			return 0;
		}
		hw_dbg(hw, "NOT RXing /C/, disable AutoNeg and force link.\n");

		/* Disable auto-negotiation in the TXCW register */
		ew32(TXCW, (mac->txcw & ~E1000_TXCW_ANE));

		/* Force link-up and also force full-duplex. */
		ctrl = er32(CTRL);
		ctrl |= (E1000_CTRL_SLU | E1000_CTRL_FD);
		ew32(CTRL, ctrl);

		/* Configure Flow Control after forcing link up. */
		ret_val = e1000e_config_fc_after_link_up(hw);
		if (ret_val) {
			hw_dbg(hw, "Error configuring flow control\n");
			return ret_val;
		}
	} else if ((ctrl & E1000_CTRL_SLU) && (rxcw & E1000_RXCW_C)) {
		/*
		 * If we are forcing link and we are receiving /C/ ordered
		 * sets, re-enable auto-negotiation in the TXCW register
		 * and disable forced link in the Device Control register
		 * in an attempt to auto-negotiate with our link partner.
		 */
		hw_dbg(hw, "RXing /C/, enable AutoNeg and stop forcing link.\n");
		ew32(TXCW, mac->txcw);
		ew32(CTRL, (ctrl & ~E1000_CTRL_SLU));

		mac->serdes_has_link = 1;
	} else if (!(E1000_TXCW_ANE & er32(TXCW))) {
		/*
		 * If we force link for non-auto-negotiation switch, check
		 * link status based on MAC synchronization for internal
		 * serdes media type.
		 */
		/* SYNCH bit and IV bit are sticky. */
		udelay(10);
		rxcw = er32(RXCW);
		if (rxcw & E1000_RXCW_SYNCH) {
			if (!(rxcw & E1000_RXCW_IV)) {
				mac->serdes_has_link = true;
				hw_dbg(hw, "SERDES: Link up - forced.\n");
			}
		} else {
			mac->serdes_has_link = false;
			hw_dbg(hw, "SERDES: Link down - force failed.\n");
		}
	}

	if (E1000_TXCW_ANE & er32(TXCW)) {
		status = er32(STATUS);
		if (status & E1000_STATUS_LU) {
			/* SYNCH bit and IV bit are sticky, so reread rxcw.  */
			udelay(10);
			rxcw = er32(RXCW);
			if (rxcw & E1000_RXCW_SYNCH) {
				if (!(rxcw & E1000_RXCW_IV)) {
					mac->serdes_has_link = true;
					hw_dbg(hw, "SERDES: Link up - autoneg "
					   "completed sucessfully.\n");
				} else {
					mac->serdes_has_link = false;
					hw_dbg(hw, "SERDES: Link down - invalid"
					   "codewords detected in autoneg.\n");
				}
			} else {
				mac->serdes_has_link = false;
				hw_dbg(hw, "SERDES: Link down - no sync.\n");
			}
		} else {
			mac->serdes_has_link = false;
			hw_dbg(hw, "SERDES: Link down - autoneg failed\n");
		}
	}

	return 0;
}

static s32 e1000_set_default_fc_generic(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 nvm_data;

	/*
	 * Read and store word 0x0F of the EEPROM. This word contains bits
	 * that determine the hardware's default PAUSE (flow control) mode,
	 * a bit that determines whether the HW defaults to enabling or
	 * disabling auto-negotiation, and the direction of the
	 * SW defined pins. If there is no SW over-ride of the flow
	 * control setting, then the variable hw->fc will
	 * be initialized based on a value in the EEPROM.
	 */
	ret_val = e1000_read_nvm(hw, NVM_INIT_CONTROL2_REG, 1, &nvm_data);

	if (ret_val) {
		hw_dbg(hw, "NVM Read Error\n");
		return ret_val;
	}

	if ((nvm_data & NVM_WORD0F_PAUSE_MASK) == 0)
		hw->fc.requested_mode = e1000_fc_none;
	else if ((nvm_data & NVM_WORD0F_PAUSE_MASK) ==
		 NVM_WORD0F_ASM_DIR)
		hw->fc.requested_mode = e1000_fc_tx_pause;
	else
		hw->fc.requested_mode = e1000_fc_full;

	return 0;
}

s32 e1000e_setup_link(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;

	/*
	 * In the case of the phy reset being blocked, we already have a link.
	 * We do not need to set it up again.
	 */
	if (e1000_check_reset_block(hw))
		return 0;

	/*
	 * If requested flow control is set to default, set flow control
	 * based on the EEPROM flow control settings.
	 */
	if (hw->fc.requested_mode == e1000_fc_default) {
		ret_val = e1000_set_default_fc_generic(hw);
		if (ret_val)
			return ret_val;
	}

	/*
	 * Save off the requested flow control mode for use later.  Depending
	 * on the link partner's capabilities, we may or may not use this mode.
	 */
	hw->fc.current_mode = hw->fc.requested_mode;

	hw_dbg(hw, "After fix-ups FlowControl is now = %x\n",
		hw->fc.current_mode);

	/* Call the necessary media_type subroutine to configure the link. */
	ret_val = mac->ops.setup_physical_interface(hw);
	if (ret_val)
		return ret_val;

	/*
	 * Initialize the flow control address, type, and PAUSE timer
	 * registers to their default values.  This is done even if flow
	 * control is disabled, because it does not hurt anything to
	 * initialize these registers.
	 */
	hw_dbg(hw, "Initializing the Flow Control address, type and timer regs\n");
	ew32(FCT, FLOW_CONTROL_TYPE);
	ew32(FCAH, FLOW_CONTROL_ADDRESS_HIGH);
	ew32(FCAL, FLOW_CONTROL_ADDRESS_LOW);

	ew32(FCTTV, hw->fc.pause_time);

	return e1000e_set_fc_watermarks(hw);
}

static s32 e1000_commit_fc_settings_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 txcw;

	/*
	 * Check for a software override of the flow control settings, and
	 * setup the device accordingly.  If auto-negotiation is enabled, then
	 * software will have to set the "PAUSE" bits to the correct value in
	 * the Transmit Config Word Register (TXCW) and re-start auto-
	 * negotiation.  However, if auto-negotiation is disabled, then
	 * software will have to manually configure the two flow control enable
	 * bits in the CTRL register.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause frames,
	 *	  but not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames but we
	 *	  do not support receiving pause frames).
	 *      3:  Both Rx and Tx flow control (symmetric) are enabled.
	 */
	switch (hw->fc.current_mode) {
	case e1000_fc_none:
		/* Flow control completely disabled by a software over-ride. */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD);
		break;
	case e1000_fc_rx_pause:
		/*
		 * Rx Flow control is enabled and Tx Flow control is disabled
		 * by a software over-ride. Since there really isn't a way to
		 * advertise that we are capable of Rx Pause ONLY, we will
		 * advertise that we support both symmetric and asymmetric Rx
		 * PAUSE.  Later, we will disable the adapter's ability to send
		 * PAUSE frames.
		 */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		break;
	case e1000_fc_tx_pause:
		/*
		 * Tx Flow control is enabled, and Rx Flow control is disabled,
		 * by a software over-ride.
		 */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_ASM_DIR);
		break;
	case e1000_fc_full:
		/*
		 * Flow control (both Rx and Tx) is enabled by a software
		 * over-ride.
		 */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		break;
	default:
		hw_dbg(hw, "Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
		break;
	}

	ew32(TXCW, txcw);
	mac->txcw = txcw;

	return 0;
}

static s32 e1000_poll_fiber_serdes_link_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 i, status;
	s32 ret_val;

	/*
	 * If we have a signal (the cable is plugged in, or assumed true for
	 * serdes media) then poll for a "Link-Up" indication in the Device
	 * Status Register.  Time-out if a link isn't seen in 500 milliseconds
	 * seconds (Auto-negotiation should complete in less than 500
	 * milliseconds even if the other end is doing it in SW).
	 */
	for (i = 0; i < FIBER_LINK_UP_LIMIT; i++) {
		msleep(10);
		status = er32(STATUS);
		if (status & E1000_STATUS_LU)
			break;
	}
	if (i == FIBER_LINK_UP_LIMIT) {
		hw_dbg(hw, "Never got a valid link from auto-neg!!!\n");
		mac->autoneg_failed = 1;
		/*
		 * AutoNeg failed to achieve a link, so we'll call
		 * mac->check_for_link. This routine will force the
		 * link up if we detect a signal. This will allow us to
		 * communicate with non-autonegotiating link partners.
		 */
		ret_val = mac->ops.check_for_link(hw);
		if (ret_val) {
			hw_dbg(hw, "Error while checking for link\n");
			return ret_val;
		}
		mac->autoneg_failed = 0;
	} else {
		mac->autoneg_failed = 0;
		hw_dbg(hw, "Valid Link Found\n");
	}

	return 0;
}

s32 e1000e_setup_fiber_serdes_link(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;

	ctrl = er32(CTRL);

	/* Take the link out of reset */
	ctrl &= ~E1000_CTRL_LRST;

	e1000e_config_collision_dist(hw);

	ret_val = e1000_commit_fc_settings_generic(hw);
	if (ret_val)
		return ret_val;

	/*
	 * Since auto-negotiation is enabled, take the link out of reset (the
	 * link will be in reset, because we previously reset the chip). This
	 * will restart auto-negotiation.  If auto-negotiation is successful
	 * then the link-up status bit will be set and the flow control enable
	 * bits (RFCE and TFCE) will be set according to their negotiated value.
	 */
	hw_dbg(hw, "Auto-negotiation enabled\n");

	ew32(CTRL, ctrl);
	e1e_flush();
	msleep(1);

	/*
	 * For these adapters, the SW definable pin 1 is set when the optics
	 * detect a signal.  If we have a signal, then poll for a "Link-Up"
	 * indication.
	 */
	if (hw->phy.media_type == e1000_media_type_internal_serdes ||
	    (er32(CTRL) & E1000_CTRL_SWDPIN1)) {
		ret_val = e1000_poll_fiber_serdes_link_generic(hw);
	} else {
		hw_dbg(hw, "No signal detected\n");
	}

	return 0;
}

void e1000e_config_collision_dist(struct e1000_hw *hw)
{
	u32 tctl;

	tctl = er32(TCTL);

	tctl &= ~E1000_TCTL_COLD;
	tctl |= E1000_COLLISION_DISTANCE << E1000_COLD_SHIFT;

	ew32(TCTL, tctl);
	e1e_flush();
}

s32 e1000e_set_fc_watermarks(struct e1000_hw *hw)
{
	u32 fcrtl = 0, fcrth = 0;

	/*
	 * Set the flow control receive threshold registers.  Normally,
	 * these registers will be set to a default threshold that may be
	 * adjusted later by the driver's runtime code.  However, if the
	 * ability to transmit pause frames is not enabled, then these
	 * registers will be set to 0.
	 */
	if (hw->fc.current_mode & e1000_fc_tx_pause) {
		/*
		 * We need to set up the Receive Threshold high and low water
		 * marks as well as (optionally) enabling the transmission of
		 * XON frames.
		 */
		fcrtl = hw->fc.low_water;
		fcrtl |= E1000_FCRTL_XONE;
		fcrth = hw->fc.high_water;
	}
	ew32(FCRTL, fcrtl);
	ew32(FCRTH, fcrth);

	return 0;
}

s32 e1000e_force_mac_fc(struct e1000_hw *hw)
{
	u32 ctrl;

	ctrl = er32(CTRL);

	/*
	 * Because we didn't get link via the internal auto-negotiation
	 * mechanism (we either forced link or we got link via PHY
	 * auto-neg), we have to manually enable/disable transmit an
	 * receive flow control.
	 *
	 * The "Case" statement below enables/disable flow control
	 * according to the "hw->fc.current_mode" parameter.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause
	 *	  frames but not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames
	 *	  frames but we do not receive pause frames).
	 *      3:  Both Rx and Tx flow control (symmetric) is enabled.
	 *  other:  No other values should be possible at this point.
	 */
	hw_dbg(hw, "hw->fc.current_mode = %u\n", hw->fc.current_mode);

	switch (hw->fc.current_mode) {
	case e1000_fc_none:
		ctrl &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
		break;
	case e1000_fc_rx_pause:
		ctrl &= (~E1000_CTRL_TFCE);
		ctrl |= E1000_CTRL_RFCE;
		break;
	case e1000_fc_tx_pause:
		ctrl &= (~E1000_CTRL_RFCE);
		ctrl |= E1000_CTRL_TFCE;
		break;
	case e1000_fc_full:
		ctrl |= (E1000_CTRL_TFCE | E1000_CTRL_RFCE);
		break;
	default:
		hw_dbg(hw, "Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
	}

	ew32(CTRL, ctrl);

	return 0;
}

s32 e1000e_config_fc_after_link_up(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val = 0;
	u16 mii_status_reg, mii_nway_adv_reg, mii_nway_lp_ability_reg;
	u16 speed, duplex;

	/*
	 * Check for the case where we have fiber media and auto-neg failed
	 * so we had to force link.  In this case, we need to force the
	 * configuration of the MAC to match the "fc" parameter.
	 */
	if (mac->autoneg_failed) {
		if (hw->phy.media_type == e1000_media_type_fiber ||
		    hw->phy.media_type == e1000_media_type_internal_serdes)
			ret_val = e1000e_force_mac_fc(hw);
	} else {
		if (hw->phy.media_type == e1000_media_type_copper)
			ret_val = e1000e_force_mac_fc(hw);
	}

	if (ret_val) {
		hw_dbg(hw, "Error forcing flow control settings\n");
		return ret_val;
	}

	/*
	 * Check for the case where we have copper media and auto-neg is
	 * enabled.  In this case, we need to check and see if Auto-Neg
	 * has completed, and if so, how the PHY and link partner has
	 * flow control configured.
	 */
	if ((hw->phy.media_type == e1000_media_type_copper) && mac->autoneg) {
		/*
		 * Read the MII Status Register and check to see if AutoNeg
		 * has completed.  We read this twice because this reg has
		 * some "sticky" (latched) bits.
		 */
		ret_val = e1e_rphy(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;
		ret_val = e1e_rphy(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;

		if (!(mii_status_reg & MII_SR_AUTONEG_COMPLETE)) {
			hw_dbg(hw, "Copper PHY and Auto Neg "
				 "has not completed.\n");
			return ret_val;
		}

		/*
		 * The AutoNeg process has completed, so we now need to
		 * read both the Auto Negotiation Advertisement
		 * Register (Address 4) and the Auto_Negotiation Base
		 * Page Ability Register (Address 5) to determine how
		 * flow control was negotiated.
		 */
		ret_val = e1e_rphy(hw, PHY_AUTONEG_ADV, &mii_nway_adv_reg);
		if (ret_val)
			return ret_val;
		ret_val = e1e_rphy(hw, PHY_LP_ABILITY, &mii_nway_lp_ability_reg);
		if (ret_val)
			return ret_val;

		/*
		 * Two bits in the Auto Negotiation Advertisement Register
		 * (Address 4) and two bits in the Auto Negotiation Base
		 * Page Ability Register (Address 5) determine flow control
		 * for both the PHY and the link partner.  The following
		 * table, taken out of the IEEE 802.3ab/D6.0 dated March 25,
		 * 1999, describes these PAUSE resolution bits and how flow
		 * control is determined based upon these settings.
		 * NOTE:  DC = Don't Care
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | NIC Resolution
		 *-------|---------|-------|---------|--------------------
		 *   0   |    0    |  DC   |   DC    | e1000_fc_none
		 *   0   |    1    |   0   |   DC    | e1000_fc_none
		 *   0   |    1    |   1   |    0    | e1000_fc_none
		 *   0   |    1    |   1   |    1    | e1000_fc_tx_pause
		 *   1   |    0    |   0   |   DC    | e1000_fc_none
		 *   1   |   DC    |   1   |   DC    | e1000_fc_full
		 *   1   |    1    |   0   |    0    | e1000_fc_none
		 *   1   |    1    |   0   |    1    | e1000_fc_rx_pause
		 *
		 *
		 * Are both PAUSE bits set to 1?  If so, this implies
		 * Symmetric Flow Control is enabled at both ends.  The
		 * ASM_DIR bits are irrelevant per the spec.
		 *
		 * For Symmetric Flow Control:
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   1   |   DC    |   1   |   DC    | E1000_fc_full
		 *
		 */
		if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
		    (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE)) {
			/*
			 * Now we need to check if the user selected Rx ONLY
			 * of pause frames.  In this case, we had to advertise
			 * FULL flow control because we could not advertise Rx
			 * ONLY. Hence, we must now check to see if we need to
			 * turn OFF  the TRANSMISSION of PAUSE frames.
			 */
			if (hw->fc.requested_mode == e1000_fc_full) {
				hw->fc.current_mode = e1000_fc_full;
				hw_dbg(hw, "Flow Control = FULL.\r\n");
			} else {
				hw->fc.current_mode = e1000_fc_rx_pause;
				hw_dbg(hw, "Flow Control = "
					 "RX PAUSE frames only.\r\n");
			}
		}
		/*
		 * For receiving PAUSE frames ONLY.
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   0   |    1    |   1   |    1    | e1000_fc_tx_pause
		 *
		 */
		else if (!(mii_nway_adv_reg & NWAY_AR_PAUSE) &&
			  (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
			  (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
			  (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
			hw->fc.current_mode = e1000_fc_tx_pause;
			hw_dbg(hw, "Flow Control = Tx PAUSE frames only.\r\n");
		}
		/*
		 * For transmitting PAUSE frames ONLY.
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   1   |    1    |   0   |    1    | e1000_fc_rx_pause
		 *
		 */
		else if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
			 (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
			 !(mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
			 (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
			hw->fc.current_mode = e1000_fc_rx_pause;
			hw_dbg(hw, "Flow Control = Rx PAUSE frames only.\r\n");
		} else {
			/*
			 * Per the IEEE spec, at this point flow control
			 * should be disabled.
			 */
			hw->fc.current_mode = e1000_fc_none;
			hw_dbg(hw, "Flow Control = NONE.\r\n");
		}

		/*
		 * Now we need to do one last check...  If we auto-
		 * negotiated to HALF DUPLEX, flow control should not be
		 * enabled per IEEE 802.3 spec.
		 */
		ret_val = mac->ops.get_link_up_info(hw, &speed, &duplex);
		if (ret_val) {
			hw_dbg(hw, "Error getting link speed and duplex\n");
			return ret_val;
		}

		if (duplex == HALF_DUPLEX)
			hw->fc.current_mode = e1000_fc_none;

		/*
		 * Now we call a subroutine to actually force the MAC
		 * controller to use the correct flow control settings.
		 */
		ret_val = e1000e_force_mac_fc(hw);
		if (ret_val) {
			hw_dbg(hw, "Error forcing flow control settings\n");
			return ret_val;
		}
	}

	return 0;
}

s32 e1000e_get_speed_and_duplex_copper(struct e1000_hw *hw, u16 *speed, u16 *duplex)
{
	u32 status;

	status = er32(STATUS);
	if (status & E1000_STATUS_SPEED_1000) {
		*speed = SPEED_1000;
		hw_dbg(hw, "1000 Mbs, ");
	} else if (status & E1000_STATUS_SPEED_100) {
		*speed = SPEED_100;
		hw_dbg(hw, "100 Mbs, ");
	} else {
		*speed = SPEED_10;
		hw_dbg(hw, "10 Mbs, ");
	}

	if (status & E1000_STATUS_FD) {
		*duplex = FULL_DUPLEX;
		hw_dbg(hw, "Full Duplex\n");
	} else {
		*duplex = HALF_DUPLEX;
		hw_dbg(hw, "Half Duplex\n");
	}

	return 0;
}

s32 e1000e_get_speed_and_duplex_fiber_serdes(struct e1000_hw *hw, u16 *speed, u16 *duplex)
{
	*speed = SPEED_1000;
	*duplex = FULL_DUPLEX;

	return 0;
}

s32 e1000e_get_hw_semaphore(struct e1000_hw *hw)
{
	u32 swsm;
	s32 timeout = hw->nvm.word_size + 1;
	s32 i = 0;

	/* Get the SW semaphore */
	while (i < timeout) {
		swsm = er32(SWSM);
		if (!(swsm & E1000_SWSM_SMBI))
			break;

		udelay(50);
		i++;
	}

	if (i == timeout) {
		hw_dbg(hw, "Driver can't access device - SMBI bit is set.\n");
		return -E1000_ERR_NVM;
	}

	/* Get the FW semaphore. */
	for (i = 0; i < timeout; i++) {
		swsm = er32(SWSM);
		ew32(SWSM, swsm | E1000_SWSM_SWESMBI);

		/* Semaphore acquired if bit latched */
		if (er32(SWSM) & E1000_SWSM_SWESMBI)
			break;

		udelay(50);
	}

	if (i == timeout) {
		/* Release semaphores */
		e1000e_put_hw_semaphore(hw);
		hw_dbg(hw, "Driver can't access the NVM\n");
		return -E1000_ERR_NVM;
	}

	return 0;
}

void e1000e_put_hw_semaphore(struct e1000_hw *hw)
{
	u32 swsm;

	swsm = er32(SWSM);
	swsm &= ~(E1000_SWSM_SMBI | E1000_SWSM_SWESMBI);
	ew32(SWSM, swsm);
}

s32 e1000e_get_auto_rd_done(struct e1000_hw *hw)
{
	s32 i = 0;

	while (i < AUTO_READ_DONE_TIMEOUT) {
		if (er32(EECD) & E1000_EECD_AUTO_RD)
			break;
		msleep(1);
		i++;
	}

	if (i == AUTO_READ_DONE_TIMEOUT) {
		hw_dbg(hw, "Auto read by HW from NVM has not completed.\n");
		return -E1000_ERR_RESET;
	}

	return 0;
}

s32 e1000e_valid_led_default(struct e1000_hw *hw, u16 *data)
{
	s32 ret_val;

	ret_val = e1000_read_nvm(hw, NVM_ID_LED_SETTINGS, 1, data);
	if (ret_val) {
		hw_dbg(hw, "NVM Read Error\n");
		return ret_val;
	}

	if (*data == ID_LED_RESERVED_0000 || *data == ID_LED_RESERVED_FFFF)
		*data = ID_LED_DEFAULT;

	return 0;
}

s32 e1000e_id_led_init(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	const u32 ledctl_mask = 0x000000FF;
	const u32 ledctl_on = E1000_LEDCTL_MODE_LED_ON;
	const u32 ledctl_off = E1000_LEDCTL_MODE_LED_OFF;
	u16 data, i, temp;
	const u16 led_mask = 0x0F;

	ret_val = hw->nvm.ops.valid_led_default(hw, &data);
	if (ret_val)
		return ret_val;

	mac->ledctl_default = er32(LEDCTL);
	mac->ledctl_mode1 = mac->ledctl_default;
	mac->ledctl_mode2 = mac->ledctl_default;

	for (i = 0; i < 4; i++) {
		temp = (data >> (i << 2)) & led_mask;
		switch (temp) {
		case ID_LED_ON1_DEF2:
		case ID_LED_ON1_ON2:
		case ID_LED_ON1_OFF2:
			mac->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode1 |= ledctl_on << (i << 3);
			break;
		case ID_LED_OFF1_DEF2:
		case ID_LED_OFF1_ON2:
		case ID_LED_OFF1_OFF2:
			mac->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode1 |= ledctl_off << (i << 3);
			break;
		default:
			/* Do nothing */
			break;
		}
		switch (temp) {
		case ID_LED_DEF1_ON2:
		case ID_LED_ON1_ON2:
		case ID_LED_OFF1_ON2:
			mac->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode2 |= ledctl_on << (i << 3);
			break;
		case ID_LED_DEF1_OFF2:
		case ID_LED_ON1_OFF2:
		case ID_LED_OFF1_OFF2:
			mac->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode2 |= ledctl_off << (i << 3);
			break;
		default:
			/* Do nothing */
			break;
		}
	}

	return 0;
}

s32 e1000e_cleanup_led_generic(struct e1000_hw *hw)
{
	ew32(LEDCTL, hw->mac.ledctl_default);
	return 0;
}

s32 e1000e_blink_led(struct e1000_hw *hw)
{
	u32 ledctl_blink = 0;
	u32 i;

	if (hw->phy.media_type == e1000_media_type_fiber) {
		/* always blink LED0 for PCI-E fiber */
		ledctl_blink = E1000_LEDCTL_LED0_BLINK |
		     (E1000_LEDCTL_MODE_LED_ON << E1000_LEDCTL_LED0_MODE_SHIFT);
	} else {
		/*
		 * set the blink bit for each LED that's "on" (0x0E)
		 * in ledctl_mode2
		 */
		ledctl_blink = hw->mac.ledctl_mode2;
		for (i = 0; i < 4; i++)
			if (((hw->mac.ledctl_mode2 >> (i * 8)) & 0xFF) ==
			    E1000_LEDCTL_MODE_LED_ON)
				ledctl_blink |= (E1000_LEDCTL_LED0_BLINK <<
						 (i * 8));
	}

	ew32(LEDCTL, ledctl_blink);

	return 0;
}

s32 e1000e_led_on_generic(struct e1000_hw *hw)
{
	u32 ctrl;

	switch (hw->phy.media_type) {
	case e1000_media_type_fiber:
		ctrl = er32(CTRL);
		ctrl &= ~E1000_CTRL_SWDPIN0;
		ctrl |= E1000_CTRL_SWDPIO0;
		ew32(CTRL, ctrl);
		break;
	case e1000_media_type_copper:
		ew32(LEDCTL, hw->mac.ledctl_mode2);
		break;
	default:
		break;
	}

	return 0;
}

s32 e1000e_led_off_generic(struct e1000_hw *hw)
{
	u32 ctrl;

	switch (hw->phy.media_type) {
	case e1000_media_type_fiber:
		ctrl = er32(CTRL);
		ctrl |= E1000_CTRL_SWDPIN0;
		ctrl |= E1000_CTRL_SWDPIO0;
		ew32(CTRL, ctrl);
		break;
	case e1000_media_type_copper:
		ew32(LEDCTL, hw->mac.ledctl_mode1);
		break;
	default:
		break;
	}

	return 0;
}

void e1000e_set_pcie_no_snoop(struct e1000_hw *hw, u32 no_snoop)
{
	u32 gcr;

	if (no_snoop) {
		gcr = er32(GCR);
		gcr &= ~(PCIE_NO_SNOOP_ALL);
		gcr |= no_snoop;
		ew32(GCR, gcr);
	}
}

s32 e1000e_disable_pcie_master(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 timeout = MASTER_DISABLE_TIMEOUT;

	ctrl = er32(CTRL);
	ctrl |= E1000_CTRL_GIO_MASTER_DISABLE;
	ew32(CTRL, ctrl);

	while (timeout) {
		if (!(er32(STATUS) &
		      E1000_STATUS_GIO_MASTER_ENABLE))
			break;
		udelay(100);
		timeout--;
	}

	if (!timeout) {
		hw_dbg(hw, "Master requests are pending.\n");
		return -E1000_ERR_MASTER_REQUESTS_PENDING;
	}

	return 0;
}

void e1000e_reset_adaptive(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;

	mac->current_ifs_val = 0;
	mac->ifs_min_val = IFS_MIN;
	mac->ifs_max_val = IFS_MAX;
	mac->ifs_step_size = IFS_STEP;
	mac->ifs_ratio = IFS_RATIO;

	mac->in_ifs_mode = 0;
	ew32(AIT, 0);
}

void e1000e_update_adaptive(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;

	if ((mac->collision_delta * mac->ifs_ratio) > mac->tx_packet_delta) {
		if (mac->tx_packet_delta > MIN_NUM_XMITS) {
			mac->in_ifs_mode = 1;
			if (mac->current_ifs_val < mac->ifs_max_val) {
				if (!mac->current_ifs_val)
					mac->current_ifs_val = mac->ifs_min_val;
				else
					mac->current_ifs_val +=
						mac->ifs_step_size;
				ew32(AIT, mac->current_ifs_val);
			}
		}
	} else {
		if (mac->in_ifs_mode &&
		    (mac->tx_packet_delta <= MIN_NUM_XMITS)) {
			mac->current_ifs_val = 0;
			mac->in_ifs_mode = 0;
			ew32(AIT, 0);
		}
	}
}

static void e1000_raise_eec_clk(struct e1000_hw *hw, u32 *eecd)
{
	*eecd = *eecd | E1000_EECD_SK;
	ew32(EECD, *eecd);
	e1e_flush();
	udelay(hw->nvm.delay_usec);
}

static void e1000_lower_eec_clk(struct e1000_hw *hw, u32 *eecd)
{
	*eecd = *eecd & ~E1000_EECD_SK;
	ew32(EECD, *eecd);
	e1e_flush();
	udelay(hw->nvm.delay_usec);
}

static void e1000_shift_out_eec_bits(struct e1000_hw *hw, u16 data, u16 count)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 eecd = er32(EECD);
	u32 mask;

	mask = 0x01 << (count - 1);
	if (nvm->type == e1000_nvm_eeprom_spi)
		eecd |= E1000_EECD_DO;

	do {
		eecd &= ~E1000_EECD_DI;

		if (data & mask)
			eecd |= E1000_EECD_DI;

		ew32(EECD, eecd);
		e1e_flush();

		udelay(nvm->delay_usec);

		e1000_raise_eec_clk(hw, &eecd);
		e1000_lower_eec_clk(hw, &eecd);

		mask >>= 1;
	} while (mask);

	eecd &= ~E1000_EECD_DI;
	ew32(EECD, eecd);
}

static u16 e1000_shift_in_eec_bits(struct e1000_hw *hw, u16 count)
{
	u32 eecd;
	u32 i;
	u16 data;

	eecd = er32(EECD);

	eecd &= ~(E1000_EECD_DO | E1000_EECD_DI);
	data = 0;

	for (i = 0; i < count; i++) {
		data <<= 1;
		e1000_raise_eec_clk(hw, &eecd);

		eecd = er32(EECD);

		eecd &= ~E1000_EECD_DI;
		if (eecd & E1000_EECD_DO)
			data |= 1;

		e1000_lower_eec_clk(hw, &eecd);
	}

	return data;
}

s32 e1000e_poll_eerd_eewr_done(struct e1000_hw *hw, int ee_reg)
{
	u32 attempts = 100000;
	u32 i, reg = 0;

	for (i = 0; i < attempts; i++) {
		if (ee_reg == E1000_NVM_POLL_READ)
			reg = er32(EERD);
		else
			reg = er32(EEWR);

		if (reg & E1000_NVM_RW_REG_DONE)
			return 0;

		udelay(5);
	}

	return -E1000_ERR_NVM;
}

s32 e1000e_acquire_nvm(struct e1000_hw *hw)
{
	u32 eecd = er32(EECD);
	s32 timeout = E1000_NVM_GRANT_ATTEMPTS;

	ew32(EECD, eecd | E1000_EECD_REQ);
	eecd = er32(EECD);

	while (timeout) {
		if (eecd & E1000_EECD_GNT)
			break;
		udelay(5);
		eecd = er32(EECD);
		timeout--;
	}

	if (!timeout) {
		eecd &= ~E1000_EECD_REQ;
		ew32(EECD, eecd);
		hw_dbg(hw, "Could not acquire NVM grant\n");
		return -E1000_ERR_NVM;
	}

	return 0;
}

static void e1000_standby_nvm(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 eecd = er32(EECD);

	if (nvm->type == e1000_nvm_eeprom_spi) {
		/* Toggle CS to flush commands */
		eecd |= E1000_EECD_CS;
		ew32(EECD, eecd);
		e1e_flush();
		udelay(nvm->delay_usec);
		eecd &= ~E1000_EECD_CS;
		ew32(EECD, eecd);
		e1e_flush();
		udelay(nvm->delay_usec);
	}
}

static void e1000_stop_nvm(struct e1000_hw *hw)
{
	u32 eecd;

	eecd = er32(EECD);
	if (hw->nvm.type == e1000_nvm_eeprom_spi) {
		/* Pull CS high */
		eecd |= E1000_EECD_CS;
		e1000_lower_eec_clk(hw, &eecd);
	}
}

void e1000e_release_nvm(struct e1000_hw *hw)
{
	u32 eecd;

	e1000_stop_nvm(hw);

	eecd = er32(EECD);
	eecd &= ~E1000_EECD_REQ;
	ew32(EECD, eecd);
}

static s32 e1000_ready_nvm_eeprom(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 eecd = er32(EECD);
	u16 timeout = 0;
	u8 spi_stat_reg;

	if (nvm->type == e1000_nvm_eeprom_spi) {
		/* Clear SK and CS */
		eecd &= ~(E1000_EECD_CS | E1000_EECD_SK);
		ew32(EECD, eecd);
		udelay(1);
		timeout = NVM_MAX_RETRY_SPI;

		/*
		 * Read "Status Register" repeatedly until the LSB is cleared.
		 * The EEPROM will signal that the command has been completed
		 * by clearing bit 0 of the internal status register.  If it's
		 * not cleared within 'timeout', then error out.
		 */
		while (timeout) {
			e1000_shift_out_eec_bits(hw, NVM_RDSR_OPCODE_SPI,
						 hw->nvm.opcode_bits);
			spi_stat_reg = (u8)e1000_shift_in_eec_bits(hw, 8);
			if (!(spi_stat_reg & NVM_STATUS_RDY_SPI))
				break;

			udelay(5);
			e1000_standby_nvm(hw);
			timeout--;
		}

		if (!timeout) {
			hw_dbg(hw, "SPI NVM Status error\n");
			return -E1000_ERR_NVM;
		}
	}

	return 0;
}

s32 e1000e_read_nvm_eerd(struct e1000_hw *hw, u16 offset, u16 words, u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 i, eerd = 0;
	s32 ret_val = 0;

	/*
	 * A check for invalid values:  offset too large, too many words,
	 * too many words for the offset, and not enough words.
	 */
	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		hw_dbg(hw, "nvm parameter(s) out of bounds\n");
		return -E1000_ERR_NVM;
	}

	for (i = 0; i < words; i++) {
		eerd = ((offset+i) << E1000_NVM_RW_ADDR_SHIFT) +
		       E1000_NVM_RW_REG_START;

		ew32(EERD, eerd);
		ret_val = e1000e_poll_eerd_eewr_done(hw, E1000_NVM_POLL_READ);
		if (ret_val)
			break;

		data[i] = (er32(EERD) >> E1000_NVM_RW_REG_DATA);
	}

	return ret_val;
}

s32 e1000e_write_nvm_spi(struct e1000_hw *hw, u16 offset, u16 words, u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	s32 ret_val;
	u16 widx = 0;

	/*
	 * A check for invalid values:  offset too large, too many words,
	 * and not enough words.
	 */
	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		hw_dbg(hw, "nvm parameter(s) out of bounds\n");
		return -E1000_ERR_NVM;
	}

	ret_val = nvm->ops.acquire_nvm(hw);
	if (ret_val)
		return ret_val;

	msleep(10);

	while (widx < words) {
		u8 write_opcode = NVM_WRITE_OPCODE_SPI;

		ret_val = e1000_ready_nvm_eeprom(hw);
		if (ret_val) {
			nvm->ops.release_nvm(hw);
			return ret_val;
		}

		e1000_standby_nvm(hw);

		/* Send the WRITE ENABLE command (8 bit opcode) */
		e1000_shift_out_eec_bits(hw, NVM_WREN_OPCODE_SPI,
					 nvm->opcode_bits);

		e1000_standby_nvm(hw);

		/*
		 * Some SPI eeproms use the 8th address bit embedded in the
		 * opcode
		 */
		if ((nvm->address_bits == 8) && (offset >= 128))
			write_opcode |= NVM_A8_OPCODE_SPI;

		/* Send the Write command (8-bit opcode + addr) */
		e1000_shift_out_eec_bits(hw, write_opcode, nvm->opcode_bits);
		e1000_shift_out_eec_bits(hw, (u16)((offset + widx) * 2),
					 nvm->address_bits);

		/* Loop to allow for up to whole page write of eeprom */
		while (widx < words) {
			u16 word_out = data[widx];
			word_out = (word_out >> 8) | (word_out << 8);
			e1000_shift_out_eec_bits(hw, word_out, 16);
			widx++;

			if ((((offset + widx) * 2) % nvm->page_size) == 0) {
				e1000_standby_nvm(hw);
				break;
			}
		}
	}

	msleep(10);
	nvm->ops.release_nvm(hw);
	return 0;
}

s32 e1000e_read_mac_addr(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 offset, nvm_data, i;
	u16 mac_addr_offset = 0;

	if (hw->mac.type == e1000_82571) {
		/* Check for an alternate MAC address.  An alternate MAC
		 * address can be setup by pre-boot software and must be
		 * treated like a permanent address and must override the
		 * actual permanent MAC address.*/
		ret_val = e1000_read_nvm(hw, NVM_ALT_MAC_ADDR_PTR, 1,
					 &mac_addr_offset);
		if (ret_val) {
			hw_dbg(hw, "NVM Read Error\n");
			return ret_val;
		}
		if (mac_addr_offset == 0xFFFF)
			mac_addr_offset = 0;

		if (mac_addr_offset) {
			if (hw->bus.func == E1000_FUNC_1)
				mac_addr_offset += ETH_ALEN/sizeof(u16);

			/* make sure we have a valid mac address here
			* before using it */
			ret_val = e1000_read_nvm(hw, mac_addr_offset, 1,
						 &nvm_data);
			if (ret_val) {
				hw_dbg(hw, "NVM Read Error\n");
				return ret_val;
			}
			if (nvm_data & 0x0001)
				mac_addr_offset = 0;
		}

		if (mac_addr_offset)
		hw->dev_spec.e82571.alt_mac_addr_is_present = 1;
	}

	for (i = 0; i < ETH_ALEN; i += 2) {
		offset = mac_addr_offset + (i >> 1);
		ret_val = e1000_read_nvm(hw, offset, 1, &nvm_data);
		if (ret_val) {
			hw_dbg(hw, "NVM Read Error\n");
			return ret_val;
		}
		hw->mac.perm_addr[i] = (u8)(nvm_data & 0xFF);
		hw->mac.perm_addr[i+1] = (u8)(nvm_data >> 8);
	}

	/* Flip last bit of mac address if we're on second port */
	if (!mac_addr_offset && hw->bus.func == E1000_FUNC_1)
		hw->mac.perm_addr[5] ^= 1;

	for (i = 0; i < ETH_ALEN; i++)
		hw->mac.addr[i] = hw->mac.perm_addr[i];

	return 0;
}

s32 e1000e_validate_nvm_checksum_generic(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 checksum = 0;
	u16 i, nvm_data;

	for (i = 0; i < (NVM_CHECKSUM_REG + 1); i++) {
		ret_val = e1000_read_nvm(hw, i, 1, &nvm_data);
		if (ret_val) {
			hw_dbg(hw, "NVM Read Error\n");
			return ret_val;
		}
		checksum += nvm_data;
	}

	if (checksum != (u16) NVM_SUM) {
		hw_dbg(hw, "NVM Checksum Invalid\n");
		return -E1000_ERR_NVM;
	}

	return 0;
}

s32 e1000e_update_nvm_checksum_generic(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 checksum = 0;
	u16 i, nvm_data;

	for (i = 0; i < NVM_CHECKSUM_REG; i++) {
		ret_val = e1000_read_nvm(hw, i, 1, &nvm_data);
		if (ret_val) {
			hw_dbg(hw, "NVM Read Error while updating checksum.\n");
			return ret_val;
		}
		checksum += nvm_data;
	}
	checksum = (u16) NVM_SUM - checksum;
	ret_val = e1000_write_nvm(hw, NVM_CHECKSUM_REG, 1, &checksum);
	if (ret_val)
		hw_dbg(hw, "NVM Write Error while updating checksum.\n");

	return ret_val;
}

void e1000e_reload_nvm(struct e1000_hw *hw)
{
	u32 ctrl_ext;

	udelay(10);
	ctrl_ext = er32(CTRL_EXT);
	ctrl_ext |= E1000_CTRL_EXT_EE_RST;
	ew32(CTRL_EXT, ctrl_ext);
	e1e_flush();
}

static u8 e1000_calculate_checksum(u8 *buffer, u32 length)
{
	u32 i;
	u8  sum = 0;

	if (!buffer)
		return 0;

	for (i = 0; i < length; i++)
		sum += buffer[i];

	return (u8) (0 - sum);
}

static s32 e1000_mng_enable_host_if(struct e1000_hw *hw)
{
	u32 hicr;
	u8 i;

	/* Check that the host interface is enabled. */
	hicr = er32(HICR);
	if ((hicr & E1000_HICR_EN) == 0) {
		hw_dbg(hw, "E1000_HOST_EN bit disabled.\n");
		return -E1000_ERR_HOST_INTERFACE_COMMAND;
	}
	/* check the previous command is completed */
	for (i = 0; i < E1000_MNG_DHCP_COMMAND_TIMEOUT; i++) {
		hicr = er32(HICR);
		if (!(hicr & E1000_HICR_C))
			break;
		mdelay(1);
	}

	if (i == E1000_MNG_DHCP_COMMAND_TIMEOUT) {
		hw_dbg(hw, "Previous command timeout failed .\n");
		return -E1000_ERR_HOST_INTERFACE_COMMAND;
	}

	return 0;
}

bool e1000e_check_mng_mode_generic(struct e1000_hw *hw)
{
	u32 fwsm = er32(FWSM);

	return (fwsm & E1000_FWSM_MODE_MASK) ==
		(E1000_MNG_IAMT_MODE << E1000_FWSM_MODE_SHIFT);
}

bool e1000e_enable_tx_pkt_filtering(struct e1000_hw *hw)
{
	struct e1000_host_mng_dhcp_cookie *hdr = &hw->mng_cookie;
	u32 *buffer = (u32 *)&hw->mng_cookie;
	u32 offset;
	s32 ret_val, hdr_csum, csum;
	u8 i, len;

	/* No manageability, no filtering */
	if (!e1000e_check_mng_mode(hw)) {
		hw->mac.tx_pkt_filtering = 0;
		return 0;
	}

	/*
	 * If we can't read from the host interface for whatever
	 * reason, disable filtering.
	 */
	ret_val = e1000_mng_enable_host_if(hw);
	if (ret_val != 0) {
		hw->mac.tx_pkt_filtering = 0;
		return ret_val;
	}

	/* Read in the header.  Length and offset are in dwords. */
	len    = E1000_MNG_DHCP_COOKIE_LENGTH >> 2;
	offset = E1000_MNG_DHCP_COOKIE_OFFSET >> 2;
	for (i = 0; i < len; i++)
		*(buffer + i) = E1000_READ_REG_ARRAY(hw, E1000_HOST_IF, offset + i);
	hdr_csum = hdr->checksum;
	hdr->checksum = 0;
	csum = e1000_calculate_checksum((u8 *)hdr,
					E1000_MNG_DHCP_COOKIE_LENGTH);
	/*
	 * If either the checksums or signature don't match, then
	 * the cookie area isn't considered valid, in which case we
	 * take the safe route of assuming Tx filtering is enabled.
	 */
	if ((hdr_csum != csum) || (hdr->signature != E1000_IAMT_SIGNATURE)) {
		hw->mac.tx_pkt_filtering = 1;
		return 1;
	}

	/* Cookie area is valid, make the final check for filtering. */
	if (!(hdr->status & E1000_MNG_DHCP_COOKIE_STATUS_PARSING)) {
		hw->mac.tx_pkt_filtering = 0;
		return 0;
	}

	hw->mac.tx_pkt_filtering = 1;
	return 1;
}

static s32 e1000_mng_write_cmd_header(struct e1000_hw *hw,
				  struct e1000_host_mng_command_header *hdr)
{
	u16 i, length = sizeof(struct e1000_host_mng_command_header);

	/* Write the whole command header structure with new checksum. */

	hdr->checksum = e1000_calculate_checksum((u8 *)hdr, length);

	length >>= 2;
	/* Write the relevant command block into the ram area. */
	for (i = 0; i < length; i++) {
		E1000_WRITE_REG_ARRAY(hw, E1000_HOST_IF, i,
					    *((u32 *) hdr + i));
		e1e_flush();
	}

	return 0;
}

static s32 e1000_mng_host_if_write(struct e1000_hw *hw, u8 *buffer,
				   u16 length, u16 offset, u8 *sum)
{
	u8 *tmp;
	u8 *bufptr = buffer;
	u32 data = 0;
	u16 remaining, i, j, prev_bytes;

	/* sum = only sum of the data and it is not checksum */

	if (length == 0 || offset + length > E1000_HI_MAX_MNG_DATA_LENGTH)
		return -E1000_ERR_PARAM;

	tmp = (u8 *)&data;
	prev_bytes = offset & 0x3;
	offset >>= 2;

	if (prev_bytes) {
		data = E1000_READ_REG_ARRAY(hw, E1000_HOST_IF, offset);
		for (j = prev_bytes; j < sizeof(u32); j++) {
			*(tmp + j) = *bufptr++;
			*sum += *(tmp + j);
		}
		E1000_WRITE_REG_ARRAY(hw, E1000_HOST_IF, offset, data);
		length -= j - prev_bytes;
		offset++;
	}

	remaining = length & 0x3;
	length -= remaining;

	/* Calculate length in DWORDs */
	length >>= 2;

	/*
	 * The device driver writes the relevant command block into the
	 * ram area.
	 */
	for (i = 0; i < length; i++) {
		for (j = 0; j < sizeof(u32); j++) {
			*(tmp + j) = *bufptr++;
			*sum += *(tmp + j);
		}

		E1000_WRITE_REG_ARRAY(hw, E1000_HOST_IF, offset + i, data);
	}
	if (remaining) {
		for (j = 0; j < sizeof(u32); j++) {
			if (j < remaining)
				*(tmp + j) = *bufptr++;
			else
				*(tmp + j) = 0;

			*sum += *(tmp + j);
		}
		E1000_WRITE_REG_ARRAY(hw, E1000_HOST_IF, offset + i, data);
	}

	return 0;
}

s32 e1000e_mng_write_dhcp_info(struct e1000_hw *hw, u8 *buffer, u16 length)
{
	struct e1000_host_mng_command_header hdr;
	s32 ret_val;
	u32 hicr;

	hdr.command_id = E1000_MNG_DHCP_TX_PAYLOAD_CMD;
	hdr.command_length = length;
	hdr.reserved1 = 0;
	hdr.reserved2 = 0;
	hdr.checksum = 0;

	/* Enable the host interface */
	ret_val = e1000_mng_enable_host_if(hw);
	if (ret_val)
		return ret_val;

	/* Populate the host interface with the contents of "buffer". */
	ret_val = e1000_mng_host_if_write(hw, buffer, length,
					  sizeof(hdr), &(hdr.checksum));
	if (ret_val)
		return ret_val;

	/* Write the manageability command header */
	ret_val = e1000_mng_write_cmd_header(hw, &hdr);
	if (ret_val)
		return ret_val;

	/* Tell the ARC a new command is pending. */
	hicr = er32(HICR);
	ew32(HICR, hicr | E1000_HICR_C);

	return 0;
}

bool e1000e_enable_mng_pass_thru(struct e1000_hw *hw)
{
	u32 manc;
	u32 fwsm, factps;
	bool ret_val = 0;

	manc = er32(MANC);

	if (!(manc & E1000_MANC_RCV_TCO_EN) ||
	    !(manc & E1000_MANC_EN_MAC_ADDR_FILTER))
		return ret_val;

	if (hw->mac.arc_subsystem_valid) {
		fwsm = er32(FWSM);
		factps = er32(FACTPS);

		if (!(factps & E1000_FACTPS_MNGCG) &&
		    ((fwsm & E1000_FWSM_MODE_MASK) ==
		     (e1000_mng_mode_pt << E1000_FWSM_MODE_SHIFT))) {
			ret_val = 1;
			return ret_val;
		}
	} else {
		if ((manc & E1000_MANC_SMBUS_EN) &&
		    !(manc & E1000_MANC_ASF_EN)) {
			ret_val = 1;
			return ret_val;
		}
	}

	return ret_val;
}

s32 e1000e_read_pba_num(struct e1000_hw *hw, u32 *pba_num)
{
	s32 ret_val;
	u16 nvm_data;

	ret_val = e1000_read_nvm(hw, NVM_PBA_OFFSET_0, 1, &nvm_data);
	if (ret_val) {
		hw_dbg(hw, "NVM Read Error\n");
		return ret_val;
	}
	*pba_num = (u32)(nvm_data << 16);

	ret_val = e1000_read_nvm(hw, NVM_PBA_OFFSET_1, 1, &nvm_data);
	if (ret_val) {
		hw_dbg(hw, "NVM Read Error\n");
		return ret_val;
	}
	*pba_num |= nvm_data;

	return 0;
}
