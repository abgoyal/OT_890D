

#include <linux/delay.h>

#include "e1000.h"

static s32 e1000_get_phy_cfg_done(struct e1000_hw *hw);
static s32 e1000_phy_force_speed_duplex(struct e1000_hw *hw);
static s32 e1000_set_d0_lplu_state(struct e1000_hw *hw, bool active);
static s32 e1000_wait_autoneg(struct e1000_hw *hw);
static u32 e1000_get_phy_addr_for_bm_page(u32 page, u32 reg);
static s32 e1000_access_phy_wakeup_reg_bm(struct e1000_hw *hw, u32 offset,
					  u16 *data, bool read);

/* Cable length tables */
static const u16 e1000_m88_cable_length_table[] =
	{ 0, 50, 80, 110, 140, 140, E1000_CABLE_LENGTH_UNDEFINED };

static const u16 e1000_igp_2_cable_length_table[] =
	{ 0, 0, 0, 0, 0, 0, 0, 0, 3, 5, 8, 11, 13, 16, 18, 21, 0, 0, 0, 3,
	  6, 10, 13, 16, 19, 23, 26, 29, 32, 35, 38, 41, 6, 10, 14, 18, 22,
	  26, 30, 33, 37, 41, 44, 48, 51, 54, 58, 61, 21, 26, 31, 35, 40,
	  44, 49, 53, 57, 61, 65, 68, 72, 75, 79, 82, 40, 45, 51, 56, 61,
	  66, 70, 75, 79, 83, 87, 91, 94, 98, 101, 104, 60, 66, 72, 77, 82,
	  87, 92, 96, 100, 104, 108, 111, 114, 117, 119, 121, 83, 89, 95,
	  100, 105, 109, 113, 116, 119, 122, 124, 104, 109, 114, 118, 121,
	  124};
#define IGP02E1000_CABLE_LENGTH_TABLE_SIZE \
		ARRAY_SIZE(e1000_igp_2_cable_length_table)

s32 e1000e_check_reset_block_generic(struct e1000_hw *hw)
{
	u32 manc;

	manc = er32(MANC);

	return (manc & E1000_MANC_BLK_PHY_RST_ON_IDE) ?
	       E1000_BLK_PHY_RESET : 0;
}

s32 e1000e_get_phy_id(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_id;

	ret_val = e1e_rphy(hw, PHY_ID1, &phy_id);
	if (ret_val)
		return ret_val;

	phy->id = (u32)(phy_id << 16);
	udelay(20);
	ret_val = e1e_rphy(hw, PHY_ID2, &phy_id);
	if (ret_val)
		return ret_val;

	phy->id |= (u32)(phy_id & PHY_REVISION_MASK);
	phy->revision = (u32)(phy_id & ~PHY_REVISION_MASK);

	return 0;
}

s32 e1000e_phy_reset_dsp(struct e1000_hw *hw)
{
	s32 ret_val;

	ret_val = e1e_wphy(hw, M88E1000_PHY_GEN_CONTROL, 0xC1);
	if (ret_val)
		return ret_val;

	return e1e_wphy(hw, M88E1000_PHY_GEN_CONTROL, 0);
}

s32 e1000e_read_phy_reg_mdic(struct e1000_hw *hw, u32 offset, u16 *data)
{
	struct e1000_phy_info *phy = &hw->phy;
	u32 i, mdic = 0;

	if (offset > MAX_PHY_REG_ADDRESS) {
		hw_dbg(hw, "PHY Address %d is out of range\n", offset);
		return -E1000_ERR_PARAM;
	}

	/*
	 * Set up Op-code, Phy Address, and register offset in the MDI
	 * Control register.  The MAC will take care of interfacing with the
	 * PHY to retrieve the desired data.
	 */
	mdic = ((offset << E1000_MDIC_REG_SHIFT) |
		(phy->addr << E1000_MDIC_PHY_SHIFT) |
		(E1000_MDIC_OP_READ));

	ew32(MDIC, mdic);

	/*
	 * Poll the ready bit to see if the MDI read completed
	 * Increasing the time out as testing showed failures with
	 * the lower time out
	 */
	for (i = 0; i < (E1000_GEN_POLL_TIMEOUT * 3); i++) {
		udelay(50);
		mdic = er32(MDIC);
		if (mdic & E1000_MDIC_READY)
			break;
	}
	if (!(mdic & E1000_MDIC_READY)) {
		hw_dbg(hw, "MDI Read did not complete\n");
		return -E1000_ERR_PHY;
	}
	if (mdic & E1000_MDIC_ERROR) {
		hw_dbg(hw, "MDI Error\n");
		return -E1000_ERR_PHY;
	}
	*data = (u16) mdic;

	return 0;
}

s32 e1000e_write_phy_reg_mdic(struct e1000_hw *hw, u32 offset, u16 data)
{
	struct e1000_phy_info *phy = &hw->phy;
	u32 i, mdic = 0;

	if (offset > MAX_PHY_REG_ADDRESS) {
		hw_dbg(hw, "PHY Address %d is out of range\n", offset);
		return -E1000_ERR_PARAM;
	}

	/*
	 * Set up Op-code, Phy Address, and register offset in the MDI
	 * Control register.  The MAC will take care of interfacing with the
	 * PHY to retrieve the desired data.
	 */
	mdic = (((u32)data) |
		(offset << E1000_MDIC_REG_SHIFT) |
		(phy->addr << E1000_MDIC_PHY_SHIFT) |
		(E1000_MDIC_OP_WRITE));

	ew32(MDIC, mdic);

	/*
	 * Poll the ready bit to see if the MDI read completed
	 * Increasing the time out as testing showed failures with
	 * the lower time out
	 */
	for (i = 0; i < (E1000_GEN_POLL_TIMEOUT * 3); i++) {
		udelay(50);
		mdic = er32(MDIC);
		if (mdic & E1000_MDIC_READY)
			break;
	}
	if (!(mdic & E1000_MDIC_READY)) {
		hw_dbg(hw, "MDI Write did not complete\n");
		return -E1000_ERR_PHY;
	}
	if (mdic & E1000_MDIC_ERROR) {
		hw_dbg(hw, "MDI Error\n");
		return -E1000_ERR_PHY;
	}

	return 0;
}

s32 e1000e_read_phy_reg_m88(struct e1000_hw *hw, u32 offset, u16 *data)
{
	s32 ret_val;

	ret_val = hw->phy.ops.acquire_phy(hw);
	if (ret_val)
		return ret_val;

	ret_val = e1000e_read_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
					   data);

	hw->phy.ops.release_phy(hw);

	return ret_val;
}

s32 e1000e_write_phy_reg_m88(struct e1000_hw *hw, u32 offset, u16 data)
{
	s32 ret_val;

	ret_val = hw->phy.ops.acquire_phy(hw);
	if (ret_val)
		return ret_val;

	ret_val = e1000e_write_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
					    data);

	hw->phy.ops.release_phy(hw);

	return ret_val;
}

s32 e1000e_read_phy_reg_igp(struct e1000_hw *hw, u32 offset, u16 *data)
{
	s32 ret_val;

	ret_val = hw->phy.ops.acquire_phy(hw);
	if (ret_val)
		return ret_val;

	if (offset > MAX_PHY_MULTI_PAGE_REG) {
		ret_val = e1000e_write_phy_reg_mdic(hw,
						    IGP01E1000_PHY_PAGE_SELECT,
						    (u16)offset);
		if (ret_val) {
			hw->phy.ops.release_phy(hw);
			return ret_val;
		}
	}

	ret_val = e1000e_read_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
					   data);

	hw->phy.ops.release_phy(hw);

	return ret_val;
}

s32 e1000e_write_phy_reg_igp(struct e1000_hw *hw, u32 offset, u16 data)
{
	s32 ret_val;

	ret_val = hw->phy.ops.acquire_phy(hw);
	if (ret_val)
		return ret_val;

	if (offset > MAX_PHY_MULTI_PAGE_REG) {
		ret_val = e1000e_write_phy_reg_mdic(hw,
						    IGP01E1000_PHY_PAGE_SELECT,
						    (u16)offset);
		if (ret_val) {
			hw->phy.ops.release_phy(hw);
			return ret_val;
		}
	}

	ret_val = e1000e_write_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
					    data);

	hw->phy.ops.release_phy(hw);

	return ret_val;
}

s32 e1000e_read_kmrn_reg(struct e1000_hw *hw, u32 offset, u16 *data)
{
	u32 kmrnctrlsta;
	s32 ret_val;

	ret_val = hw->phy.ops.acquire_phy(hw);
	if (ret_val)
		return ret_val;

	kmrnctrlsta = ((offset << E1000_KMRNCTRLSTA_OFFSET_SHIFT) &
		       E1000_KMRNCTRLSTA_OFFSET) | E1000_KMRNCTRLSTA_REN;
	ew32(KMRNCTRLSTA, kmrnctrlsta);

	udelay(2);

	kmrnctrlsta = er32(KMRNCTRLSTA);
	*data = (u16)kmrnctrlsta;

	hw->phy.ops.release_phy(hw);

	return ret_val;
}

s32 e1000e_write_kmrn_reg(struct e1000_hw *hw, u32 offset, u16 data)
{
	u32 kmrnctrlsta;
	s32 ret_val;

	ret_val = hw->phy.ops.acquire_phy(hw);
	if (ret_val)
		return ret_val;

	kmrnctrlsta = ((offset << E1000_KMRNCTRLSTA_OFFSET_SHIFT) &
		       E1000_KMRNCTRLSTA_OFFSET) | data;
	ew32(KMRNCTRLSTA, kmrnctrlsta);

	udelay(2);
	hw->phy.ops.release_phy(hw);

	return ret_val;
}

s32 e1000e_copper_link_setup_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;

	/* Enable CRS on Tx. This must be set for half-duplex operation. */
	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	/* For newer PHYs this bit is downshift enable */
	if (phy->type == e1000_phy_m88)
		phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;

	/*
	 * Options:
	 *   MDI/MDI-X = 0 (default)
	 *   0 - Auto for all speeds
	 *   1 - MDI mode
	 *   2 - MDI-X mode
	 *   3 - Auto for 1000Base-T only (MDI-X for 10/100Base-T modes)
	 */
	phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;

	switch (phy->mdix) {
	case 1:
		phy_data |= M88E1000_PSCR_MDI_MANUAL_MODE;
		break;
	case 2:
		phy_data |= M88E1000_PSCR_MDIX_MANUAL_MODE;
		break;
	case 3:
		phy_data |= M88E1000_PSCR_AUTO_X_1000T;
		break;
	case 0:
	default:
		phy_data |= M88E1000_PSCR_AUTO_X_MODE;
		break;
	}

	/*
	 * Options:
	 *   disable_polarity_correction = 0 (default)
	 *       Automatic Correction for Reversed Cable Polarity
	 *   0 - Disabled
	 *   1 - Enabled
	 */
	phy_data &= ~M88E1000_PSCR_POLARITY_REVERSAL;
	if (phy->disable_polarity_correction == 1)
		phy_data |= M88E1000_PSCR_POLARITY_REVERSAL;

	/* Enable downshift on BM (disabled by default) */
	if (phy->type == e1000_phy_bm)
		phy_data |= BME1000_PSCR_ENABLE_DOWNSHIFT;

	ret_val = e1e_wphy(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	if ((phy->type == e1000_phy_m88) &&
	    (phy->revision < E1000_REVISION_4) &&
	    (phy->id != BME1000_E_PHY_ID_R2)) {
		/*
		 * Force TX_CLK in the Extended PHY Specific Control Register
		 * to 25MHz clock.
		 */
		ret_val = e1e_rphy(hw, M88E1000_EXT_PHY_SPEC_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		phy_data |= M88E1000_EPSCR_TX_CLK_25;

		if ((phy->revision == 2) &&
		    (phy->id == M88E1111_I_PHY_ID)) {
			/* 82573L PHY - set the downshift counter to 5x. */
			phy_data &= ~M88EC018_EPSCR_DOWNSHIFT_COUNTER_MASK;
			phy_data |= M88EC018_EPSCR_DOWNSHIFT_COUNTER_5X;
		} else {
			/* Configure Master and Slave downshift values */
			phy_data &= ~(M88E1000_EPSCR_MASTER_DOWNSHIFT_MASK |
				      M88E1000_EPSCR_SLAVE_DOWNSHIFT_MASK);
			phy_data |= (M88E1000_EPSCR_MASTER_DOWNSHIFT_1X |
				     M88E1000_EPSCR_SLAVE_DOWNSHIFT_1X);
		}
		ret_val = e1e_wphy(hw, M88E1000_EXT_PHY_SPEC_CTRL, phy_data);
		if (ret_val)
			return ret_val;
	}

	if ((phy->type == e1000_phy_bm) && (phy->id == BME1000_E_PHY_ID_R2)) {
		/* Set PHY page 0, register 29 to 0x0003 */
		ret_val = e1e_wphy(hw, 29, 0x0003);
		if (ret_val)
			return ret_val;

		/* Set PHY page 0, register 30 to 0x0000 */
		ret_val = e1e_wphy(hw, 30, 0x0000);
		if (ret_val)
			return ret_val;
	}

	/* Commit the changes. */
	ret_val = e1000e_commit_phy(hw);
	if (ret_val)
		hw_dbg(hw, "Error committing the PHY changes\n");

	return ret_val;
}

s32 e1000e_copper_link_setup_igp(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	ret_val = e1000_phy_hw_reset(hw);
	if (ret_val) {
		hw_dbg(hw, "Error resetting the PHY.\n");
		return ret_val;
	}

	/*
	 * Wait 100ms for MAC to configure PHY from NVM settings, to avoid
	 * timeout issues when LFS is enabled.
	 */
	msleep(100);

	/* disable lplu d0 during driver init */
	ret_val = e1000_set_d0_lplu_state(hw, 0);
	if (ret_val) {
		hw_dbg(hw, "Error Disabling LPLU D0\n");
		return ret_val;
	}
	/* Configure mdi-mdix settings */
	ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CTRL, &data);
	if (ret_val)
		return ret_val;

	data &= ~IGP01E1000_PSCR_AUTO_MDIX;

	switch (phy->mdix) {
	case 1:
		data &= ~IGP01E1000_PSCR_FORCE_MDI_MDIX;
		break;
	case 2:
		data |= IGP01E1000_PSCR_FORCE_MDI_MDIX;
		break;
	case 0:
	default:
		data |= IGP01E1000_PSCR_AUTO_MDIX;
		break;
	}
	ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CTRL, data);
	if (ret_val)
		return ret_val;

	/* set auto-master slave resolution settings */
	if (hw->mac.autoneg) {
		/*
		 * when autonegotiation advertisement is only 1000Mbps then we
		 * should disable SmartSpeed and enable Auto MasterSlave
		 * resolution as hardware default.
		 */
		if (phy->autoneg_advertised == ADVERTISE_1000_FULL) {
			/* Disable SmartSpeed */
			ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CONFIG,
					   &data);
			if (ret_val)
				return ret_val;

			data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CONFIG,
					   data);
			if (ret_val)
				return ret_val;

			/* Set auto Master/Slave resolution process */
			ret_val = e1e_rphy(hw, PHY_1000T_CTRL, &data);
			if (ret_val)
				return ret_val;

			data &= ~CR_1000T_MS_ENABLE;
			ret_val = e1e_wphy(hw, PHY_1000T_CTRL, data);
			if (ret_val)
				return ret_val;
		}

		ret_val = e1e_rphy(hw, PHY_1000T_CTRL, &data);
		if (ret_val)
			return ret_val;

		/* load defaults for future use */
		phy->original_ms_type = (data & CR_1000T_MS_ENABLE) ?
			((data & CR_1000T_MS_VALUE) ?
			e1000_ms_force_master :
			e1000_ms_force_slave) :
			e1000_ms_auto;

		switch (phy->ms_type) {
		case e1000_ms_force_master:
			data |= (CR_1000T_MS_ENABLE | CR_1000T_MS_VALUE);
			break;
		case e1000_ms_force_slave:
			data |= CR_1000T_MS_ENABLE;
			data &= ~(CR_1000T_MS_VALUE);
			break;
		case e1000_ms_auto:
			data &= ~CR_1000T_MS_ENABLE;
		default:
			break;
		}
		ret_val = e1e_wphy(hw, PHY_1000T_CTRL, data);
	}

	return ret_val;
}

static s32 e1000_phy_setup_autoneg(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 mii_autoneg_adv_reg;
	u16 mii_1000t_ctrl_reg = 0;

	phy->autoneg_advertised &= phy->autoneg_mask;

	/* Read the MII Auto-Neg Advertisement Register (Address 4). */
	ret_val = e1e_rphy(hw, PHY_AUTONEG_ADV, &mii_autoneg_adv_reg);
	if (ret_val)
		return ret_val;

	if (phy->autoneg_mask & ADVERTISE_1000_FULL) {
		/* Read the MII 1000Base-T Control Register (Address 9). */
		ret_val = e1e_rphy(hw, PHY_1000T_CTRL, &mii_1000t_ctrl_reg);
		if (ret_val)
			return ret_val;
	}

	/*
	 * Need to parse both autoneg_advertised and fc and set up
	 * the appropriate PHY registers.  First we will parse for
	 * autoneg_advertised software override.  Since we can advertise
	 * a plethora of combinations, we need to check each bit
	 * individually.
	 */

	/*
	 * First we clear all the 10/100 mb speed bits in the Auto-Neg
	 * Advertisement Register (Address 4) and the 1000 mb speed bits in
	 * the  1000Base-T Control Register (Address 9).
	 */
	mii_autoneg_adv_reg &= ~(NWAY_AR_100TX_FD_CAPS |
				 NWAY_AR_100TX_HD_CAPS |
				 NWAY_AR_10T_FD_CAPS   |
				 NWAY_AR_10T_HD_CAPS);
	mii_1000t_ctrl_reg &= ~(CR_1000T_HD_CAPS | CR_1000T_FD_CAPS);

	hw_dbg(hw, "autoneg_advertised %x\n", phy->autoneg_advertised);

	/* Do we want to advertise 10 Mb Half Duplex? */
	if (phy->autoneg_advertised & ADVERTISE_10_HALF) {
		hw_dbg(hw, "Advertise 10mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_HD_CAPS;
	}

	/* Do we want to advertise 10 Mb Full Duplex? */
	if (phy->autoneg_advertised & ADVERTISE_10_FULL) {
		hw_dbg(hw, "Advertise 10mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_FD_CAPS;
	}

	/* Do we want to advertise 100 Mb Half Duplex? */
	if (phy->autoneg_advertised & ADVERTISE_100_HALF) {
		hw_dbg(hw, "Advertise 100mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_HD_CAPS;
	}

	/* Do we want to advertise 100 Mb Full Duplex? */
	if (phy->autoneg_advertised & ADVERTISE_100_FULL) {
		hw_dbg(hw, "Advertise 100mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_FD_CAPS;
	}

	/* We do not allow the Phy to advertise 1000 Mb Half Duplex */
	if (phy->autoneg_advertised & ADVERTISE_1000_HALF)
		hw_dbg(hw, "Advertise 1000mb Half duplex request denied!\n");

	/* Do we want to advertise 1000 Mb Full Duplex? */
	if (phy->autoneg_advertised & ADVERTISE_1000_FULL) {
		hw_dbg(hw, "Advertise 1000mb Full duplex\n");
		mii_1000t_ctrl_reg |= CR_1000T_FD_CAPS;
	}

	/*
	 * Check for a software override of the flow control settings, and
	 * setup the PHY advertisement registers accordingly.  If
	 * auto-negotiation is enabled, then software will have to set the
	 * "PAUSE" bits to the correct value in the Auto-Negotiation
	 * Advertisement Register (PHY_AUTONEG_ADV) and re-start auto-
	 * negotiation.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause frames
	 *	  but not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames
	 *	  but we do not support receiving pause frames).
	 *      3:  Both Rx and Tx flow control (symmetric) are enabled.
	 *  other:  No software override.  The flow control configuration
	 *	  in the EEPROM is used.
	 */
	switch (hw->fc.current_mode) {
	case e1000_fc_none:
		/*
		 * Flow control (Rx & Tx) is completely disabled by a
		 * software over-ride.
		 */
		mii_autoneg_adv_reg &= ~(NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	case e1000_fc_rx_pause:
		/*
		 * Rx Flow control is enabled, and Tx Flow control is
		 * disabled, by a software over-ride.
		 *
		 * Since there really isn't a way to advertise that we are
		 * capable of Rx Pause ONLY, we will advertise that we
		 * support both symmetric and asymmetric Rx PAUSE.  Later
		 * (in e1000e_config_fc_after_link_up) we will disable the
		 * hw's ability to send PAUSE frames.
		 */
		mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	case e1000_fc_tx_pause:
		/*
		 * Tx Flow control is enabled, and Rx Flow control is
		 * disabled, by a software over-ride.
		 */
		mii_autoneg_adv_reg |= NWAY_AR_ASM_DIR;
		mii_autoneg_adv_reg &= ~NWAY_AR_PAUSE;
		break;
	case e1000_fc_full:
		/*
		 * Flow control (both Rx and Tx) is enabled by a software
		 * over-ride.
		 */
		mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	default:
		hw_dbg(hw, "Flow control param set incorrectly\n");
		ret_val = -E1000_ERR_CONFIG;
		return ret_val;
	}

	ret_val = e1e_wphy(hw, PHY_AUTONEG_ADV, mii_autoneg_adv_reg);
	if (ret_val)
		return ret_val;

	hw_dbg(hw, "Auto-Neg Advertising %x\n", mii_autoneg_adv_reg);

	if (phy->autoneg_mask & ADVERTISE_1000_FULL) {
		ret_val = e1e_wphy(hw, PHY_1000T_CTRL, mii_1000t_ctrl_reg);
	}

	return ret_val;
}

static s32 e1000_copper_link_autoneg(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_ctrl;

	/*
	 * Perform some bounds checking on the autoneg advertisement
	 * parameter.
	 */
	phy->autoneg_advertised &= phy->autoneg_mask;

	/*
	 * If autoneg_advertised is zero, we assume it was not defaulted
	 * by the calling code so we set to advertise full capability.
	 */
	if (phy->autoneg_advertised == 0)
		phy->autoneg_advertised = phy->autoneg_mask;

	hw_dbg(hw, "Reconfiguring auto-neg advertisement params\n");
	ret_val = e1000_phy_setup_autoneg(hw);
	if (ret_val) {
		hw_dbg(hw, "Error Setting up Auto-Negotiation\n");
		return ret_val;
	}
	hw_dbg(hw, "Restarting Auto-Neg\n");

	/*
	 * Restart auto-negotiation by setting the Auto Neg Enable bit and
	 * the Auto Neg Restart bit in the PHY control register.
	 */
	ret_val = e1e_rphy(hw, PHY_CONTROL, &phy_ctrl);
	if (ret_val)
		return ret_val;

	phy_ctrl |= (MII_CR_AUTO_NEG_EN | MII_CR_RESTART_AUTO_NEG);
	ret_val = e1e_wphy(hw, PHY_CONTROL, phy_ctrl);
	if (ret_val)
		return ret_val;

	/*
	 * Does the user want to wait for Auto-Neg to complete here, or
	 * check at a later time (for example, callback routine).
	 */
	if (phy->autoneg_wait_to_complete) {
		ret_val = e1000_wait_autoneg(hw);
		if (ret_val) {
			hw_dbg(hw, "Error while waiting for "
				 "autoneg to complete\n");
			return ret_val;
		}
	}

	hw->mac.get_link_status = 1;

	return ret_val;
}

s32 e1000e_setup_copper_link(struct e1000_hw *hw)
{
	s32 ret_val;
	bool link;

	if (hw->mac.autoneg) {
		/*
		 * Setup autoneg and flow control advertisement and perform
		 * autonegotiation.
		 */
		ret_val = e1000_copper_link_autoneg(hw);
		if (ret_val)
			return ret_val;
	} else {
		/*
		 * PHY will be set to 10H, 10F, 100H or 100F
		 * depending on user settings.
		 */
		hw_dbg(hw, "Forcing Speed and Duplex\n");
		ret_val = e1000_phy_force_speed_duplex(hw);
		if (ret_val) {
			hw_dbg(hw, "Error Forcing Speed and Duplex\n");
			return ret_val;
		}
	}

	/*
	 * Check link status. Wait up to 100 microseconds for link to become
	 * valid.
	 */
	ret_val = e1000e_phy_has_link_generic(hw,
					     COPPER_LINK_UP_LIMIT,
					     10,
					     &link);
	if (ret_val)
		return ret_val;

	if (link) {
		hw_dbg(hw, "Valid link established!!!\n");
		e1000e_config_collision_dist(hw);
		ret_val = e1000e_config_fc_after_link_up(hw);
	} else {
		hw_dbg(hw, "Unable to establish link!!!\n");
	}

	return ret_val;
}

s32 e1000e_phy_force_speed_duplex_igp(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;
	bool link;

	ret_val = e1e_rphy(hw, PHY_CONTROL, &phy_data);
	if (ret_val)
		return ret_val;

	e1000e_phy_force_speed_duplex_setup(hw, &phy_data);

	ret_val = e1e_wphy(hw, PHY_CONTROL, phy_data);
	if (ret_val)
		return ret_val;

	/*
	 * Clear Auto-Crossover to force MDI manually.  IGP requires MDI
	 * forced whenever speed and duplex are forced.
	 */
	ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data &= ~IGP01E1000_PSCR_AUTO_MDIX;
	phy_data &= ~IGP01E1000_PSCR_FORCE_MDI_MDIX;

	ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	hw_dbg(hw, "IGP PSCR: %X\n", phy_data);

	udelay(1);

	if (phy->autoneg_wait_to_complete) {
		hw_dbg(hw, "Waiting for forced speed/duplex link on IGP phy.\n");

		ret_val = e1000e_phy_has_link_generic(hw,
						     PHY_FORCE_LIMIT,
						     100000,
						     &link);
		if (ret_val)
			return ret_val;

		if (!link)
			hw_dbg(hw, "Link taking longer than expected.\n");

		/* Try once more */
		ret_val = e1000e_phy_has_link_generic(hw,
						     PHY_FORCE_LIMIT,
						     100000,
						     &link);
		if (ret_val)
			return ret_val;
	}

	return ret_val;
}

s32 e1000e_phy_force_speed_duplex_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;
	bool link;

	/*
	 * Clear Auto-Crossover to force MDI manually.  M88E1000 requires MDI
	 * forced whenever speed and duplex are forced.
	 */
	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;
	ret_val = e1e_wphy(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	hw_dbg(hw, "M88E1000 PSCR: %X\n", phy_data);

	ret_val = e1e_rphy(hw, PHY_CONTROL, &phy_data);
	if (ret_val)
		return ret_val;

	e1000e_phy_force_speed_duplex_setup(hw, &phy_data);

	ret_val = e1e_wphy(hw, PHY_CONTROL, phy_data);
	if (ret_val)
		return ret_val;

	/* Reset the phy to commit changes. */
	ret_val = e1000e_commit_phy(hw);
	if (ret_val)
		return ret_val;

	if (phy->autoneg_wait_to_complete) {
		hw_dbg(hw, "Waiting for forced speed/duplex link on M88 phy.\n");

		ret_val = e1000e_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
						     100000, &link);
		if (ret_val)
			return ret_val;

		if (!link) {
			/*
			 * We didn't get link.
			 * Reset the DSP and cross our fingers.
			 */
			ret_val = e1e_wphy(hw, M88E1000_PHY_PAGE_SELECT,
					   0x001d);
			if (ret_val)
				return ret_val;
			ret_val = e1000e_phy_reset_dsp(hw);
			if (ret_val)
				return ret_val;
		}

		/* Try once more */
		ret_val = e1000e_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
						     100000, &link);
		if (ret_val)
			return ret_val;
	}

	ret_val = e1e_rphy(hw, M88E1000_EXT_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	/*
	 * Resetting the phy means we need to re-force TX_CLK in the
	 * Extended PHY Specific Control Register to 25MHz clock from
	 * the reset value of 2.5MHz.
	 */
	phy_data |= M88E1000_EPSCR_TX_CLK_25;
	ret_val = e1e_wphy(hw, M88E1000_EXT_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	/*
	 * In addition, we must re-enable CRS on Tx for both half and full
	 * duplex.
	 */
	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;
	ret_val = e1e_wphy(hw, M88E1000_PHY_SPEC_CTRL, phy_data);

	return ret_val;
}

void e1000e_phy_force_speed_duplex_setup(struct e1000_hw *hw, u16 *phy_ctrl)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 ctrl;

	/* Turn off flow control when forcing speed/duplex */
	hw->fc.current_mode = e1000_fc_none;

	/* Force speed/duplex on the mac */
	ctrl = er32(CTRL);
	ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	ctrl &= ~E1000_CTRL_SPD_SEL;

	/* Disable Auto Speed Detection */
	ctrl &= ~E1000_CTRL_ASDE;

	/* Disable autoneg on the phy */
	*phy_ctrl &= ~MII_CR_AUTO_NEG_EN;

	/* Forcing Full or Half Duplex? */
	if (mac->forced_speed_duplex & E1000_ALL_HALF_DUPLEX) {
		ctrl &= ~E1000_CTRL_FD;
		*phy_ctrl &= ~MII_CR_FULL_DUPLEX;
		hw_dbg(hw, "Half Duplex\n");
	} else {
		ctrl |= E1000_CTRL_FD;
		*phy_ctrl |= MII_CR_FULL_DUPLEX;
		hw_dbg(hw, "Full Duplex\n");
	}

	/* Forcing 10mb or 100mb? */
	if (mac->forced_speed_duplex & E1000_ALL_100_SPEED) {
		ctrl |= E1000_CTRL_SPD_100;
		*phy_ctrl |= MII_CR_SPEED_100;
		*phy_ctrl &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_10);
		hw_dbg(hw, "Forcing 100mb\n");
	} else {
		ctrl &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);
		*phy_ctrl |= MII_CR_SPEED_10;
		*phy_ctrl &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_100);
		hw_dbg(hw, "Forcing 10mb\n");
	}

	e1000e_config_collision_dist(hw);

	ew32(CTRL, ctrl);
}

s32 e1000e_set_d3_lplu_state(struct e1000_hw *hw, bool active)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	ret_val = e1e_rphy(hw, IGP02E1000_PHY_POWER_MGMT, &data);
	if (ret_val)
		return ret_val;

	if (!active) {
		data &= ~IGP02E1000_PM_D3_LPLU;
		ret_val = e1e_wphy(hw, IGP02E1000_PHY_POWER_MGMT, data);
		if (ret_val)
			return ret_val;
		/*
		 * LPLU and SmartSpeed are mutually exclusive.  LPLU is used
		 * during Dx states where the power conservation is most
		 * important.  During driver activity we should enable
		 * SmartSpeed, so performance is maintained.
		 */
		if (phy->smart_speed == e1000_smart_speed_on) {
			ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CONFIG,
					   &data);
			if (ret_val)
				return ret_val;

			data |= IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CONFIG,
					   data);
			if (ret_val)
				return ret_val;
		} else if (phy->smart_speed == e1000_smart_speed_off) {
			ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CONFIG,
					   &data);
			if (ret_val)
				return ret_val;

			data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CONFIG,
					   data);
			if (ret_val)
				return ret_val;
		}
	} else if ((phy->autoneg_advertised == E1000_ALL_SPEED_DUPLEX) ||
		   (phy->autoneg_advertised == E1000_ALL_NOT_GIG) ||
		   (phy->autoneg_advertised == E1000_ALL_10_SPEED)) {
		data |= IGP02E1000_PM_D3_LPLU;
		ret_val = e1e_wphy(hw, IGP02E1000_PHY_POWER_MGMT, data);
		if (ret_val)
			return ret_val;

		/* When LPLU is enabled, we should disable SmartSpeed */
		ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CONFIG, &data);
		if (ret_val)
			return ret_val;

		data &= ~IGP01E1000_PSCFR_SMART_SPEED;
		ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CONFIG, data);
	}

	return ret_val;
}

s32 e1000e_check_downshift(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data, offset, mask;

	switch (phy->type) {
	case e1000_phy_m88:
	case e1000_phy_gg82563:
		offset	= M88E1000_PHY_SPEC_STATUS;
		mask	= M88E1000_PSSR_DOWNSHIFT;
		break;
	case e1000_phy_igp_2:
	case e1000_phy_igp_3:
		offset	= IGP01E1000_PHY_LINK_HEALTH;
		mask	= IGP01E1000_PLHR_SS_DOWNGRADE;
		break;
	default:
		/* speed downshift not supported */
		phy->speed_downgraded = 0;
		return 0;
	}

	ret_val = e1e_rphy(hw, offset, &phy_data);

	if (!ret_val)
		phy->speed_downgraded = (phy_data & mask);

	return ret_val;
}

static s32 e1000_check_polarity_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_STATUS, &data);

	if (!ret_val)
		phy->cable_polarity = (data & M88E1000_PSSR_REV_POLARITY)
				      ? e1000_rev_polarity_reversed
				      : e1000_rev_polarity_normal;

	return ret_val;
}

static s32 e1000_check_polarity_igp(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data, offset, mask;

	/*
	 * Polarity is determined based on the speed of
	 * our connection.
	 */
	ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_STATUS, &data);
	if (ret_val)
		return ret_val;

	if ((data & IGP01E1000_PSSR_SPEED_MASK) ==
	    IGP01E1000_PSSR_SPEED_1000MBPS) {
		offset	= IGP01E1000_PHY_PCS_INIT_REG;
		mask	= IGP01E1000_PHY_POLARITY_MASK;
	} else {
		/*
		 * This really only applies to 10Mbps since
		 * there is no polarity for 100Mbps (always 0).
		 */
		offset	= IGP01E1000_PHY_PORT_STATUS;
		mask	= IGP01E1000_PSSR_POLARITY_REVERSED;
	}

	ret_val = e1e_rphy(hw, offset, &data);

	if (!ret_val)
		phy->cable_polarity = (data & mask)
				      ? e1000_rev_polarity_reversed
				      : e1000_rev_polarity_normal;

	return ret_val;
}

static s32 e1000_wait_autoneg(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u16 i, phy_status;

	/* Break after autoneg completes or PHY_AUTO_NEG_LIMIT expires. */
	for (i = PHY_AUTO_NEG_LIMIT; i > 0; i--) {
		ret_val = e1e_rphy(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		ret_val = e1e_rphy(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		if (phy_status & MII_SR_AUTONEG_COMPLETE)
			break;
		msleep(100);
	}

	/*
	 * PHY_AUTO_NEG_TIME expiration doesn't guarantee auto-negotiation
	 * has completed.
	 */
	return ret_val;
}

s32 e1000e_phy_has_link_generic(struct e1000_hw *hw, u32 iterations,
			       u32 usec_interval, bool *success)
{
	s32 ret_val = 0;
	u16 i, phy_status;

	for (i = 0; i < iterations; i++) {
		/*
		 * Some PHYs require the PHY_STATUS register to be read
		 * twice due to the link bit being sticky.  No harm doing
		 * it across the board.
		 */
		ret_val = e1e_rphy(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		ret_val = e1e_rphy(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		if (phy_status & MII_SR_LINK_STATUS)
			break;
		if (usec_interval >= 1000)
			mdelay(usec_interval/1000);
		else
			udelay(usec_interval);
	}

	*success = (i < iterations);

	return ret_val;
}

s32 e1000e_get_cable_length_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data, index;

	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	index = (phy_data & M88E1000_PSSR_CABLE_LENGTH) >>
		M88E1000_PSSR_CABLE_LENGTH_SHIFT;
	phy->min_cable_length = e1000_m88_cable_length_table[index];
	phy->max_cable_length = e1000_m88_cable_length_table[index+1];

	phy->cable_length = (phy->min_cable_length + phy->max_cable_length) / 2;

	return ret_val;
}

s32 e1000e_get_cable_length_igp_2(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data, i, agc_value = 0;
	u16 cur_agc_index, max_agc_index = 0;
	u16 min_agc_index = IGP02E1000_CABLE_LENGTH_TABLE_SIZE - 1;
	u16 agc_reg_array[IGP02E1000_PHY_CHANNEL_NUM] =
							 {IGP02E1000_PHY_AGC_A,
							  IGP02E1000_PHY_AGC_B,
							  IGP02E1000_PHY_AGC_C,
							  IGP02E1000_PHY_AGC_D};

	/* Read the AGC registers for all channels */
	for (i = 0; i < IGP02E1000_PHY_CHANNEL_NUM; i++) {
		ret_val = e1e_rphy(hw, agc_reg_array[i], &phy_data);
		if (ret_val)
			return ret_val;

		/*
		 * Getting bits 15:9, which represent the combination of
		 * course and fine gain values.  The result is a number
		 * that can be put into the lookup table to obtain the
		 * approximate cable length.
		 */
		cur_agc_index = (phy_data >> IGP02E1000_AGC_LENGTH_SHIFT) &
				IGP02E1000_AGC_LENGTH_MASK;

		/* Array index bound check. */
		if ((cur_agc_index >= IGP02E1000_CABLE_LENGTH_TABLE_SIZE) ||
		    (cur_agc_index == 0))
			return -E1000_ERR_PHY;

		/* Remove min & max AGC values from calculation. */
		if (e1000_igp_2_cable_length_table[min_agc_index] >
		    e1000_igp_2_cable_length_table[cur_agc_index])
			min_agc_index = cur_agc_index;
		if (e1000_igp_2_cable_length_table[max_agc_index] <
		    e1000_igp_2_cable_length_table[cur_agc_index])
			max_agc_index = cur_agc_index;

		agc_value += e1000_igp_2_cable_length_table[cur_agc_index];
	}

	agc_value -= (e1000_igp_2_cable_length_table[min_agc_index] +
		      e1000_igp_2_cable_length_table[max_agc_index]);
	agc_value /= (IGP02E1000_PHY_CHANNEL_NUM - 2);

	/* Calculate cable length with the error range of +/- 10 meters. */
	phy->min_cable_length = ((agc_value - IGP02E1000_AGC_RANGE) > 0) ?
				 (agc_value - IGP02E1000_AGC_RANGE) : 0;
	phy->max_cable_length = agc_value + IGP02E1000_AGC_RANGE;

	phy->cable_length = (phy->min_cable_length + phy->max_cable_length) / 2;

	return ret_val;
}

s32 e1000e_get_phy_info_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32  ret_val;
	u16 phy_data;
	bool link;

	if (hw->phy.media_type != e1000_media_type_copper) {
		hw_dbg(hw, "Phy info is only valid for copper media\n");
		return -E1000_ERR_CONFIG;
	}

	ret_val = e1000e_phy_has_link_generic(hw, 1, 0, &link);
	if (ret_val)
		return ret_val;

	if (!link) {
		hw_dbg(hw, "Phy info is only valid if link is up\n");
		return -E1000_ERR_CONFIG;
	}

	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy->polarity_correction = (phy_data &
				    M88E1000_PSCR_POLARITY_REVERSAL);

	ret_val = e1000_check_polarity_m88(hw);
	if (ret_val)
		return ret_val;

	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	phy->is_mdix = (phy_data & M88E1000_PSSR_MDIX);

	if ((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_1000MBS) {
		ret_val = e1000_get_cable_length(hw);
		if (ret_val)
			return ret_val;

		ret_val = e1e_rphy(hw, PHY_1000T_STATUS, &phy_data);
		if (ret_val)
			return ret_val;

		phy->local_rx = (phy_data & SR_1000T_LOCAL_RX_STATUS)
				? e1000_1000t_rx_status_ok
				: e1000_1000t_rx_status_not_ok;

		phy->remote_rx = (phy_data & SR_1000T_REMOTE_RX_STATUS)
				 ? e1000_1000t_rx_status_ok
				 : e1000_1000t_rx_status_not_ok;
	} else {
		/* Set values to "undefined" */
		phy->cable_length = E1000_CABLE_LENGTH_UNDEFINED;
		phy->local_rx = e1000_1000t_rx_status_undefined;
		phy->remote_rx = e1000_1000t_rx_status_undefined;
	}

	return ret_val;
}

s32 e1000e_get_phy_info_igp(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;
	bool link;

	ret_val = e1000e_phy_has_link_generic(hw, 1, 0, &link);
	if (ret_val)
		return ret_val;

	if (!link) {
		hw_dbg(hw, "Phy info is only valid if link is up\n");
		return -E1000_ERR_CONFIG;
	}

	phy->polarity_correction = 1;

	ret_val = e1000_check_polarity_igp(hw);
	if (ret_val)
		return ret_val;

	ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_STATUS, &data);
	if (ret_val)
		return ret_val;

	phy->is_mdix = (data & IGP01E1000_PSSR_MDIX);

	if ((data & IGP01E1000_PSSR_SPEED_MASK) ==
	    IGP01E1000_PSSR_SPEED_1000MBPS) {
		ret_val = e1000_get_cable_length(hw);
		if (ret_val)
			return ret_val;

		ret_val = e1e_rphy(hw, PHY_1000T_STATUS, &data);
		if (ret_val)
			return ret_val;

		phy->local_rx = (data & SR_1000T_LOCAL_RX_STATUS)
				? e1000_1000t_rx_status_ok
				: e1000_1000t_rx_status_not_ok;

		phy->remote_rx = (data & SR_1000T_REMOTE_RX_STATUS)
				 ? e1000_1000t_rx_status_ok
				 : e1000_1000t_rx_status_not_ok;
	} else {
		phy->cable_length = E1000_CABLE_LENGTH_UNDEFINED;
		phy->local_rx = e1000_1000t_rx_status_undefined;
		phy->remote_rx = e1000_1000t_rx_status_undefined;
	}

	return ret_val;
}

s32 e1000e_phy_sw_reset(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 phy_ctrl;

	ret_val = e1e_rphy(hw, PHY_CONTROL, &phy_ctrl);
	if (ret_val)
		return ret_val;

	phy_ctrl |= MII_CR_RESET;
	ret_val = e1e_wphy(hw, PHY_CONTROL, phy_ctrl);
	if (ret_val)
		return ret_val;

	udelay(1);

	return ret_val;
}

s32 e1000e_phy_hw_reset_generic(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u32 ctrl;

	ret_val = e1000_check_reset_block(hw);
	if (ret_val)
		return 0;

	ret_val = phy->ops.acquire_phy(hw);
	if (ret_val)
		return ret_val;

	ctrl = er32(CTRL);
	ew32(CTRL, ctrl | E1000_CTRL_PHY_RST);
	e1e_flush();

	udelay(phy->reset_delay_us);

	ew32(CTRL, ctrl);
	e1e_flush();

	udelay(150);

	phy->ops.release_phy(hw);

	return e1000_get_phy_cfg_done(hw);
}

s32 e1000e_get_cfg_done(struct e1000_hw *hw)
{
	mdelay(10);
	return 0;
}

s32 e1000e_phy_init_script_igp3(struct e1000_hw *hw)
{
	hw_dbg(hw, "Running IGP 3 PHY init script\n");

	/* PHY init IGP 3 */
	/* Enable rise/fall, 10-mode work in class-A */
	e1e_wphy(hw, 0x2F5B, 0x9018);
	/* Remove all caps from Replica path filter */
	e1e_wphy(hw, 0x2F52, 0x0000);
	/* Bias trimming for ADC, AFE and Driver (Default) */
	e1e_wphy(hw, 0x2FB1, 0x8B24);
	/* Increase Hybrid poly bias */
	e1e_wphy(hw, 0x2FB2, 0xF8F0);
	/* Add 4% to Tx amplitude in Gig mode */
	e1e_wphy(hw, 0x2010, 0x10B0);
	/* Disable trimming (TTT) */
	e1e_wphy(hw, 0x2011, 0x0000);
	/* Poly DC correction to 94.6% + 2% for all channels */
	e1e_wphy(hw, 0x20DD, 0x249A);
	/* ABS DC correction to 95.9% */
	e1e_wphy(hw, 0x20DE, 0x00D3);
	/* BG temp curve trim */
	e1e_wphy(hw, 0x28B4, 0x04CE);
	/* Increasing ADC OPAMP stage 1 currents to max */
	e1e_wphy(hw, 0x2F70, 0x29E4);
	/* Force 1000 ( required for enabling PHY regs configuration) */
	e1e_wphy(hw, 0x0000, 0x0140);
	/* Set upd_freq to 6 */
	e1e_wphy(hw, 0x1F30, 0x1606);
	/* Disable NPDFE */
	e1e_wphy(hw, 0x1F31, 0xB814);
	/* Disable adaptive fixed FFE (Default) */
	e1e_wphy(hw, 0x1F35, 0x002A);
	/* Enable FFE hysteresis */
	e1e_wphy(hw, 0x1F3E, 0x0067);
	/* Fixed FFE for short cable lengths */
	e1e_wphy(hw, 0x1F54, 0x0065);
	/* Fixed FFE for medium cable lengths */
	e1e_wphy(hw, 0x1F55, 0x002A);
	/* Fixed FFE for long cable lengths */
	e1e_wphy(hw, 0x1F56, 0x002A);
	/* Enable Adaptive Clip Threshold */
	e1e_wphy(hw, 0x1F72, 0x3FB0);
	/* AHT reset limit to 1 */
	e1e_wphy(hw, 0x1F76, 0xC0FF);
	/* Set AHT master delay to 127 msec */
	e1e_wphy(hw, 0x1F77, 0x1DEC);
	/* Set scan bits for AHT */
	e1e_wphy(hw, 0x1F78, 0xF9EF);
	/* Set AHT Preset bits */
	e1e_wphy(hw, 0x1F79, 0x0210);
	/* Change integ_factor of channel A to 3 */
	e1e_wphy(hw, 0x1895, 0x0003);
	/* Change prop_factor of channels BCD to 8 */
	e1e_wphy(hw, 0x1796, 0x0008);
	/* Change cg_icount + enable integbp for channels BCD */
	e1e_wphy(hw, 0x1798, 0xD008);
	/*
	 * Change cg_icount + enable integbp + change prop_factor_master
	 * to 8 for channel A
	 */
	e1e_wphy(hw, 0x1898, 0xD918);
	/* Disable AHT in Slave mode on channel A */
	e1e_wphy(hw, 0x187A, 0x0800);
	/*
	 * Enable LPLU and disable AN to 1000 in non-D0a states,
	 * Enable SPD+B2B
	 */
	e1e_wphy(hw, 0x0019, 0x008D);
	/* Enable restart AN on an1000_dis change */
	e1e_wphy(hw, 0x001B, 0x2080);
	/* Enable wh_fifo read clock in 10/100 modes */
	e1e_wphy(hw, 0x0014, 0x0045);
	/* Restart AN, Speed selection is 1000 */
	e1e_wphy(hw, 0x0000, 0x1340);

	return 0;
}

/* Internal function pointers */

static s32 e1000_get_phy_cfg_done(struct e1000_hw *hw)
{
	if (hw->phy.ops.get_cfg_done)
		return hw->phy.ops.get_cfg_done(hw);

	return 0;
}

static s32 e1000_phy_force_speed_duplex(struct e1000_hw *hw)
{
	if (hw->phy.ops.force_speed_duplex)
		return hw->phy.ops.force_speed_duplex(hw);

	return 0;
}

enum e1000_phy_type e1000e_get_phy_type_from_id(u32 phy_id)
{
	enum e1000_phy_type phy_type = e1000_phy_unknown;

	switch (phy_id) {
	case M88E1000_I_PHY_ID:
	case M88E1000_E_PHY_ID:
	case M88E1111_I_PHY_ID:
	case M88E1011_I_PHY_ID:
		phy_type = e1000_phy_m88;
		break;
	case IGP01E1000_I_PHY_ID: /* IGP 1 & 2 share this */
		phy_type = e1000_phy_igp_2;
		break;
	case GG82563_E_PHY_ID:
		phy_type = e1000_phy_gg82563;
		break;
	case IGP03E1000_E_PHY_ID:
		phy_type = e1000_phy_igp_3;
		break;
	case IFE_E_PHY_ID:
	case IFE_PLUS_E_PHY_ID:
	case IFE_C_E_PHY_ID:
		phy_type = e1000_phy_ife;
		break;
	case BME1000_E_PHY_ID:
	case BME1000_E_PHY_ID_R2:
		phy_type = e1000_phy_bm;
		break;
	default:
		phy_type = e1000_phy_unknown;
		break;
	}
	return phy_type;
}

s32 e1000e_determine_phy_address(struct e1000_hw *hw)
{
	s32 ret_val = -E1000_ERR_PHY_TYPE;
	u32 phy_addr= 0;
	u32 i = 0;
	enum e1000_phy_type phy_type = e1000_phy_unknown;

	do {
		for (phy_addr = 0; phy_addr < 4; phy_addr++) {
			hw->phy.addr = phy_addr;
			e1000e_get_phy_id(hw);
			phy_type = e1000e_get_phy_type_from_id(hw->phy.id);

			/* 
			 * If phy_type is valid, break - we found our
			 * PHY address
			 */
			if (phy_type  != e1000_phy_unknown) {
				ret_val = 0;
				break;
			}
		}
		i++;
	} while ((ret_val != 0) && (i < 100));

	return ret_val;
}

static u32 e1000_get_phy_addr_for_bm_page(u32 page, u32 reg)
{
	u32 phy_addr = 2;

	if ((page >= 768) || (page == 0 && reg == 25) || (reg == 31))
		phy_addr = 1;

	return phy_addr;
}

s32 e1000e_write_phy_reg_bm(struct e1000_hw *hw, u32 offset, u16 data)
{
	s32 ret_val;
	u32 page_select = 0;
	u32 page = offset >> IGP_PAGE_SHIFT;
	u32 page_shift = 0;

	/* Page 800 works differently than the rest so it has its own func */
	if (page == BM_WUC_PAGE) {
		ret_val = e1000_access_phy_wakeup_reg_bm(hw, offset, &data,
							 false);
		goto out;
	}

	ret_val = hw->phy.ops.acquire_phy(hw);
	if (ret_val)
		goto out;

	hw->phy.addr = e1000_get_phy_addr_for_bm_page(page, offset);

	if (offset > MAX_PHY_MULTI_PAGE_REG) {
		/*
		 * Page select is register 31 for phy address 1 and 22 for
		 * phy address 2 and 3. Page select is shifted only for
		 * phy address 1.
		 */
		if (hw->phy.addr == 1) {
			page_shift = IGP_PAGE_SHIFT;
			page_select = IGP01E1000_PHY_PAGE_SELECT;
		} else {
			page_shift = 0;
			page_select = BM_PHY_PAGE_SELECT;
		}

		/* Page is shifted left, PHY expects (page x 32) */
		ret_val = e1000e_write_phy_reg_mdic(hw, page_select,
		                                    (page << page_shift));
		if (ret_val) {
			hw->phy.ops.release_phy(hw);
			goto out;
		}
	}

	ret_val = e1000e_write_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
	                                    data);

	hw->phy.ops.release_phy(hw);

out:
	return ret_val;
}

s32 e1000e_read_phy_reg_bm(struct e1000_hw *hw, u32 offset, u16 *data)
{
	s32 ret_val;
	u32 page_select = 0;
	u32 page = offset >> IGP_PAGE_SHIFT;
	u32 page_shift = 0;

	/* Page 800 works differently than the rest so it has its own func */
	if (page == BM_WUC_PAGE) {
		ret_val = e1000_access_phy_wakeup_reg_bm(hw, offset, data,
							 true);
		goto out;
	}

	ret_val = hw->phy.ops.acquire_phy(hw);
	if (ret_val)
		goto out;

	hw->phy.addr = e1000_get_phy_addr_for_bm_page(page, offset);

	if (offset > MAX_PHY_MULTI_PAGE_REG) {
		/*
		 * Page select is register 31 for phy address 1 and 22 for
		 * phy address 2 and 3. Page select is shifted only for
		 * phy address 1.
		 */
		if (hw->phy.addr == 1) {
			page_shift = IGP_PAGE_SHIFT;
			page_select = IGP01E1000_PHY_PAGE_SELECT;
		} else {
			page_shift = 0;
			page_select = BM_PHY_PAGE_SELECT;
		}

		/* Page is shifted left, PHY expects (page x 32) */
		ret_val = e1000e_write_phy_reg_mdic(hw, page_select,
		                                    (page << page_shift));
		if (ret_val) {
			hw->phy.ops.release_phy(hw);
			goto out;
		}
	}

	ret_val = e1000e_read_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
	                                   data);
	hw->phy.ops.release_phy(hw);

out:
	return ret_val;
}

s32 e1000e_read_phy_reg_bm2(struct e1000_hw *hw, u32 offset, u16 *data)
{
	s32 ret_val;
	u16 page = (u16)(offset >> IGP_PAGE_SHIFT);

	/* Page 800 works differently than the rest so it has its own func */
	if (page == BM_WUC_PAGE) {
		ret_val = e1000_access_phy_wakeup_reg_bm(hw, offset, data,
							 true);
		return ret_val;
	}

	ret_val = hw->phy.ops.acquire_phy(hw);
	if (ret_val)
		return ret_val;

	hw->phy.addr = 1;

	if (offset > MAX_PHY_MULTI_PAGE_REG) {

		/* Page is shifted left, PHY expects (page x 32) */
		ret_val = e1000e_write_phy_reg_mdic(hw, BM_PHY_PAGE_SELECT,
						    page);

		if (ret_val) {
			hw->phy.ops.release_phy(hw);
			return ret_val;
		}
	}

	ret_val = e1000e_read_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
					   data);
	hw->phy.ops.release_phy(hw);

	return ret_val;
}

s32 e1000e_write_phy_reg_bm2(struct e1000_hw *hw, u32 offset, u16 data)
{
	s32 ret_val;
	u16 page = (u16)(offset >> IGP_PAGE_SHIFT);

	/* Page 800 works differently than the rest so it has its own func */
	if (page == BM_WUC_PAGE) {
		ret_val = e1000_access_phy_wakeup_reg_bm(hw, offset, &data,
							 false);
		return ret_val;
	}

	ret_val = hw->phy.ops.acquire_phy(hw);
	if (ret_val)
		return ret_val;

	hw->phy.addr = 1;

	if (offset > MAX_PHY_MULTI_PAGE_REG) {
		/* Page is shifted left, PHY expects (page x 32) */
		ret_val = e1000e_write_phy_reg_mdic(hw, BM_PHY_PAGE_SELECT,
						    page);

		if (ret_val) {
			hw->phy.ops.release_phy(hw);
			return ret_val;
		}
	}

	ret_val = e1000e_write_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
					    data);

	hw->phy.ops.release_phy(hw);

	return ret_val;
}

static s32 e1000_access_phy_wakeup_reg_bm(struct e1000_hw *hw, u32 offset,
					  u16 *data, bool read)
{
	s32 ret_val;
	u16 reg = ((u16)offset) & PHY_REG_MASK;
	u16 phy_reg = 0;
	u8  phy_acquired = 1;


	ret_val = hw->phy.ops.acquire_phy(hw);
	if (ret_val) {
		phy_acquired = 0;
		goto out;
	}

	/* All operations in this function are phy address 1 */
	hw->phy.addr = 1;

	/* Set page 769 */
	e1000e_write_phy_reg_mdic(hw, IGP01E1000_PHY_PAGE_SELECT,
	                          (BM_WUC_ENABLE_PAGE << IGP_PAGE_SHIFT));

	ret_val = e1000e_read_phy_reg_mdic(hw, BM_WUC_ENABLE_REG, &phy_reg);
	if (ret_val)
		goto out;

	/* First clear bit 4 to avoid a power state change */
	phy_reg &= ~(BM_WUC_HOST_WU_BIT);
	ret_val = e1000e_write_phy_reg_mdic(hw, BM_WUC_ENABLE_REG, phy_reg);
	if (ret_val)
		goto out;

	/* Write bit 2 = 1, and clear bit 4 to 769_17 */
	ret_val = e1000e_write_phy_reg_mdic(hw, BM_WUC_ENABLE_REG,
	                                    phy_reg | BM_WUC_ENABLE_BIT);
	if (ret_val)
		goto out;

	/* Select page 800 */
	ret_val = e1000e_write_phy_reg_mdic(hw, IGP01E1000_PHY_PAGE_SELECT,
	                                    (BM_WUC_PAGE << IGP_PAGE_SHIFT));

	/* Write the page 800 offset value using opcode 0x11 */
	ret_val = e1000e_write_phy_reg_mdic(hw, BM_WUC_ADDRESS_OPCODE, reg);
	if (ret_val)
		goto out;

	if (read) {
	        /* Read the page 800 value using opcode 0x12 */
		ret_val = e1000e_read_phy_reg_mdic(hw, BM_WUC_DATA_OPCODE,
		                                   data);
	} else {
	        /* Read the page 800 value using opcode 0x12 */
		ret_val = e1000e_write_phy_reg_mdic(hw, BM_WUC_DATA_OPCODE,
						    *data);
	}

	if (ret_val)
		goto out;

	/*
	 * Restore 769_17.2 to its original value
	 * Set page 769
	 */
	e1000e_write_phy_reg_mdic(hw, IGP01E1000_PHY_PAGE_SELECT,
	                          (BM_WUC_ENABLE_PAGE << IGP_PAGE_SHIFT));

	/* Clear 769_17.2 */
	ret_val = e1000e_write_phy_reg_mdic(hw, BM_WUC_ENABLE_REG, phy_reg);

out:
	if (phy_acquired == 1)
		hw->phy.ops.release_phy(hw);
	return ret_val;
}

s32 e1000e_commit_phy(struct e1000_hw *hw)
{
	if (hw->phy.ops.commit_phy)
		return hw->phy.ops.commit_phy(hw);

	return 0;
}

static s32 e1000_set_d0_lplu_state(struct e1000_hw *hw, bool active)
{
	if (hw->phy.ops.set_d0_lplu_state)
		return hw->phy.ops.set_d0_lplu_state(hw, active);

	return 0;
}
