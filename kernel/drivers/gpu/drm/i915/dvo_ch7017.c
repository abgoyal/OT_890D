

#include "dvo.h"

#define CH7017_TV_DISPLAY_MODE		0x00
#define CH7017_FLICKER_FILTER		0x01
#define CH7017_VIDEO_BANDWIDTH		0x02
#define CH7017_TEXT_ENHANCEMENT		0x03
#define CH7017_START_ACTIVE_VIDEO	0x04
#define CH7017_HORIZONTAL_POSITION	0x05
#define CH7017_VERTICAL_POSITION	0x06
#define CH7017_BLACK_LEVEL		0x07
#define CH7017_CONTRAST_ENHANCEMENT	0x08
#define CH7017_TV_PLL			0x09
#define CH7017_TV_PLL_M			0x0a
#define CH7017_TV_PLL_N			0x0b
#define CH7017_SUB_CARRIER_0		0x0c
#define CH7017_CIV_CONTROL		0x10
#define CH7017_CIV_0			0x11
#define CH7017_CHROMA_BOOST		0x14
#define CH7017_CLOCK_MODE		0x1c
#define CH7017_INPUT_CLOCK		0x1d
#define CH7017_GPIO_CONTROL		0x1e
#define CH7017_INPUT_DATA_FORMAT	0x1f
#define CH7017_CONNECTION_DETECT	0x20
#define CH7017_DAC_CONTROL		0x21
#define CH7017_BUFFERED_CLOCK_OUTPUT	0x22
#define CH7017_DEFEAT_VSYNC		0x47
#define CH7017_TEST_PATTERN		0x48

#define CH7017_POWER_MANAGEMENT		0x49
/** Enables the TV output path. */
#define CH7017_TV_EN			(1 << 0)
#define CH7017_DAC0_POWER_DOWN		(1 << 1)
#define CH7017_DAC1_POWER_DOWN		(1 << 2)
#define CH7017_DAC2_POWER_DOWN		(1 << 3)
#define CH7017_DAC3_POWER_DOWN		(1 << 4)
/** Powers down the TV out block, and DAC0-3 */
#define CH7017_TV_POWER_DOWN_EN		(1 << 5)

#define CH7017_VERSION_ID		0x4a

#define CH7017_DEVICE_ID		0x4b
#define CH7017_DEVICE_ID_VALUE		0x1b
#define CH7018_DEVICE_ID_VALUE		0x1a
#define CH7019_DEVICE_ID_VALUE		0x19

#define CH7017_XCLK_D2_ADJUST		0x53
#define CH7017_UP_SCALER_COEFF_0	0x55
#define CH7017_UP_SCALER_COEFF_1	0x56
#define CH7017_UP_SCALER_COEFF_2	0x57
#define CH7017_UP_SCALER_COEFF_3	0x58
#define CH7017_UP_SCALER_COEFF_4	0x59
#define CH7017_UP_SCALER_VERTICAL_INC_0	0x5a
#define CH7017_UP_SCALER_VERTICAL_INC_1	0x5b
#define CH7017_GPIO_INVERT		0x5c
#define CH7017_UP_SCALER_HORIZONTAL_INC_0	0x5d
#define CH7017_UP_SCALER_HORIZONTAL_INC_1	0x5e

#define CH7017_HORIZONTAL_ACTIVE_PIXEL_INPUT	0x5f
/**< Low bits of horizontal active pixel input */

#define CH7017_ACTIVE_INPUT_LINE_OUTPUT	0x60
/** High bits of horizontal active pixel input */
#define CH7017_LVDS_HAP_INPUT_MASK	(0x7 << 0)
/** High bits of vertical active line output */
#define CH7017_LVDS_VAL_HIGH_MASK	(0x7 << 3)

#define CH7017_VERTICAL_ACTIVE_LINE_OUTPUT	0x61
/**< Low bits of vertical active line output */

#define CH7017_HORIZONTAL_ACTIVE_PIXEL_OUTPUT	0x62
/**< Low bits of horizontal active pixel output */

#define CH7017_LVDS_POWER_DOWN		0x63
/** High bits of horizontal active pixel output */
#define CH7017_LVDS_HAP_HIGH_MASK	(0x7 << 0)
/** Enables the LVDS power down state transition */
#define CH7017_LVDS_POWER_DOWN_EN	(1 << 6)
/** Enables the LVDS upscaler */
#define CH7017_LVDS_UPSCALER_EN		(1 << 7)
#define CH7017_LVDS_POWER_DOWN_DEFAULT_RESERVED 0x08

#define CH7017_LVDS_ENCODING		0x64
#define CH7017_LVDS_DITHER_2D		(1 << 2)
#define CH7017_LVDS_DITHER_DIS		(1 << 3)
#define CH7017_LVDS_DUAL_CHANNEL_EN	(1 << 4)
#define CH7017_LVDS_24_BIT		(1 << 5)

#define CH7017_LVDS_ENCODING_2		0x65

#define CH7017_LVDS_PLL_CONTROL		0x66
/** Enables the LVDS panel output path */
#define CH7017_LVDS_PANEN		(1 << 0)
/** Enables the LVDS panel backlight */
#define CH7017_LVDS_BKLEN		(1 << 3)

#define CH7017_POWER_SEQUENCING_T1	0x67
#define CH7017_POWER_SEQUENCING_T2	0x68
#define CH7017_POWER_SEQUENCING_T3	0x69
#define CH7017_POWER_SEQUENCING_T4	0x6a
#define CH7017_POWER_SEQUENCING_T5	0x6b
#define CH7017_GPIO_DRIVER_TYPE		0x6c
#define CH7017_GPIO_DATA		0x6d
#define CH7017_GPIO_DIRECTION_CONTROL	0x6e

#define CH7017_LVDS_PLL_FEEDBACK_DIV	0x71
# define CH7017_LVDS_PLL_FEED_BACK_DIVIDER_SHIFT 4
# define CH7017_LVDS_PLL_FEED_FORWARD_DIVIDER_SHIFT 0
# define CH7017_LVDS_PLL_FEEDBACK_DEFAULT_RESERVED 0x80

#define CH7017_LVDS_PLL_VCO_CONTROL	0x72
# define CH7017_LVDS_PLL_VCO_DEFAULT_RESERVED 0x80
# define CH7017_LVDS_PLL_VCO_SHIFT	4
# define CH7017_LVDS_PLL_POST_SCALE_DIV_SHIFT 0

#define CH7017_OUTPUTS_ENABLE		0x73
# define CH7017_CHARGE_PUMP_LOW		0x0
# define CH7017_CHARGE_PUMP_HIGH	0x3
# define CH7017_LVDS_CHANNEL_A		(1 << 3)
# define CH7017_LVDS_CHANNEL_B		(1 << 4)
# define CH7017_TV_DAC_A		(1 << 5)
# define CH7017_TV_DAC_B		(1 << 6)
# define CH7017_DDC_SELECT_DC2		(1 << 7)

#define CH7017_LVDS_OUTPUT_AMPLITUDE	0x74
#define CH7017_LVDS_PLL_EMI_REDUCTION	0x75
#define CH7017_LVDS_POWER_DOWN_FLICKER	0x76

#define CH7017_LVDS_CONTROL_2		0x78
# define CH7017_LOOP_FILTER_SHIFT	5
# define CH7017_PHASE_DETECTOR_SHIFT	0

#define CH7017_BANG_LIMIT_CONTROL	0x7f

struct ch7017_priv {
	uint8_t save_hapi;
	uint8_t save_vali;
	uint8_t save_valo;
	uint8_t save_ailo;
	uint8_t save_lvds_pll_vco;
	uint8_t save_feedback_div;
	uint8_t save_lvds_control_2;
	uint8_t save_outputs_enable;
	uint8_t save_lvds_power_down;
	uint8_t save_power_management;
};

static void ch7017_dump_regs(struct intel_dvo_device *dvo);
static void ch7017_dpms(struct intel_dvo_device *dvo, int mode);

static bool ch7017_read(struct intel_dvo_device *dvo, int addr, uint8_t *val)
{
	struct intel_i2c_chan *i2cbus = dvo->i2c_bus;
	u8 out_buf[2];
	u8 in_buf[2];

	struct i2c_msg msgs[] = {
		{
			.addr = i2cbus->slave_addr,
			.flags = 0,
			.len = 1,
			.buf = out_buf,
		},
		{
			.addr = i2cbus->slave_addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = in_buf,
		}
	};

	out_buf[0] = addr;
	out_buf[1] = 0;

	if (i2c_transfer(&i2cbus->adapter, msgs, 2) == 2) {
		*val= in_buf[0];
		return true;
	};

	return false;
}

static bool ch7017_write(struct intel_dvo_device *dvo, int addr, uint8_t val)
{
	struct intel_i2c_chan *i2cbus = dvo->i2c_bus;
	uint8_t out_buf[2];
	struct i2c_msg msg = {
		.addr = i2cbus->slave_addr,
		.flags = 0,
		.len = 2,
		.buf = out_buf,
	};

	out_buf[0] = addr;
	out_buf[1] = val;

	if (i2c_transfer(&i2cbus->adapter, &msg, 1) == 1)
		return true;

	return false;
}

/** Probes for a CH7017 on the given bus and slave address. */
static bool ch7017_init(struct intel_dvo_device *dvo,
			struct intel_i2c_chan *i2cbus)
{
	struct ch7017_priv *priv;
	uint8_t val;

	priv = kzalloc(sizeof(struct ch7017_priv), GFP_KERNEL);
	if (priv == NULL)
		return false;

	dvo->i2c_bus = i2cbus;
	dvo->i2c_bus->slave_addr = dvo->slave_addr;
	dvo->dev_priv = priv;

	if (!ch7017_read(dvo, CH7017_DEVICE_ID, &val))
		goto fail;

	if (val != CH7017_DEVICE_ID_VALUE &&
	    val != CH7018_DEVICE_ID_VALUE &&
	    val != CH7019_DEVICE_ID_VALUE) {
		DRM_DEBUG("ch701x not detected, got %d: from %s Slave %d.\n",
			  val, i2cbus->adapter.name,i2cbus->slave_addr);
		goto fail;
	}

	return true;
fail:
	kfree(priv);
	return false;
}

static enum drm_connector_status ch7017_detect(struct intel_dvo_device *dvo)
{
	return connector_status_unknown;
}

static enum drm_mode_status ch7017_mode_valid(struct intel_dvo_device *dvo,
					      struct drm_display_mode *mode)
{
	if (mode->clock > 160000)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

static void ch7017_mode_set(struct intel_dvo_device *dvo,
			    struct drm_display_mode *mode,
			    struct drm_display_mode *adjusted_mode)
{
	uint8_t lvds_pll_feedback_div, lvds_pll_vco_control;
	uint8_t outputs_enable, lvds_control_2, lvds_power_down;
	uint8_t horizontal_active_pixel_input;
	uint8_t horizontal_active_pixel_output, vertical_active_line_output;
	uint8_t active_input_line_output;

	DRM_DEBUG("Registers before mode setting\n");
	ch7017_dump_regs(dvo);

	/* LVDS PLL settings from page 75 of 7017-7017ds.pdf*/
	if (mode->clock < 100000) {
		outputs_enable = CH7017_LVDS_CHANNEL_A | CH7017_CHARGE_PUMP_LOW;
		lvds_pll_feedback_div = CH7017_LVDS_PLL_FEEDBACK_DEFAULT_RESERVED |
			(2 << CH7017_LVDS_PLL_FEED_BACK_DIVIDER_SHIFT) |
			(13 << CH7017_LVDS_PLL_FEED_FORWARD_DIVIDER_SHIFT);
		lvds_pll_vco_control = CH7017_LVDS_PLL_VCO_DEFAULT_RESERVED |
			(2 << CH7017_LVDS_PLL_VCO_SHIFT) |
			(3 << CH7017_LVDS_PLL_POST_SCALE_DIV_SHIFT);
		lvds_control_2 = (1 << CH7017_LOOP_FILTER_SHIFT) |
			(0 << CH7017_PHASE_DETECTOR_SHIFT);
	} else {
		outputs_enable = CH7017_LVDS_CHANNEL_A | CH7017_CHARGE_PUMP_HIGH;
		lvds_pll_feedback_div = CH7017_LVDS_PLL_FEEDBACK_DEFAULT_RESERVED |
			(2 << CH7017_LVDS_PLL_FEED_BACK_DIVIDER_SHIFT) |
			(3 << CH7017_LVDS_PLL_FEED_FORWARD_DIVIDER_SHIFT);
		lvds_pll_feedback_div = 35;
		lvds_control_2 = (3 << CH7017_LOOP_FILTER_SHIFT) |
			(0 << CH7017_PHASE_DETECTOR_SHIFT);
		if (1) { /* XXX: dual channel panel detection.  Assume yes for now. */
			outputs_enable |= CH7017_LVDS_CHANNEL_B;
			lvds_pll_vco_control = CH7017_LVDS_PLL_VCO_DEFAULT_RESERVED |
				(2 << CH7017_LVDS_PLL_VCO_SHIFT) |
				(13 << CH7017_LVDS_PLL_POST_SCALE_DIV_SHIFT);
		} else {
			lvds_pll_vco_control = CH7017_LVDS_PLL_VCO_DEFAULT_RESERVED |
				(1 << CH7017_LVDS_PLL_VCO_SHIFT) |
				(13 << CH7017_LVDS_PLL_POST_SCALE_DIV_SHIFT);
		}
	}

	horizontal_active_pixel_input = mode->hdisplay & 0x00ff;

	vertical_active_line_output = mode->vdisplay & 0x00ff;
	horizontal_active_pixel_output = mode->hdisplay & 0x00ff;

	active_input_line_output = ((mode->hdisplay & 0x0700) >> 8) |
				   (((mode->vdisplay & 0x0700) >> 8) << 3);

	lvds_power_down = CH7017_LVDS_POWER_DOWN_DEFAULT_RESERVED |
			  (mode->hdisplay & 0x0700) >> 8;

	ch7017_dpms(dvo, DRM_MODE_DPMS_OFF);
	ch7017_write(dvo, CH7017_HORIZONTAL_ACTIVE_PIXEL_INPUT,
			horizontal_active_pixel_input);
	ch7017_write(dvo, CH7017_HORIZONTAL_ACTIVE_PIXEL_OUTPUT,
			horizontal_active_pixel_output);
	ch7017_write(dvo, CH7017_VERTICAL_ACTIVE_LINE_OUTPUT,
			vertical_active_line_output);
	ch7017_write(dvo, CH7017_ACTIVE_INPUT_LINE_OUTPUT,
			active_input_line_output);
	ch7017_write(dvo, CH7017_LVDS_PLL_VCO_CONTROL, lvds_pll_vco_control);
	ch7017_write(dvo, CH7017_LVDS_PLL_FEEDBACK_DIV, lvds_pll_feedback_div);
	ch7017_write(dvo, CH7017_LVDS_CONTROL_2, lvds_control_2);
	ch7017_write(dvo, CH7017_OUTPUTS_ENABLE, outputs_enable);

	/* Turn the LVDS back on with new settings. */
	ch7017_write(dvo, CH7017_LVDS_POWER_DOWN, lvds_power_down);

	DRM_DEBUG("Registers after mode setting\n");
	ch7017_dump_regs(dvo);
}

/* set the CH7017 power state */
static void ch7017_dpms(struct intel_dvo_device *dvo, int mode)
{
	uint8_t val;

	ch7017_read(dvo, CH7017_LVDS_POWER_DOWN, &val);

	/* Turn off TV/VGA, and never turn it on since we don't support it. */
	ch7017_write(dvo, CH7017_POWER_MANAGEMENT,
			CH7017_DAC0_POWER_DOWN |
			CH7017_DAC1_POWER_DOWN |
			CH7017_DAC2_POWER_DOWN |
			CH7017_DAC3_POWER_DOWN |
			CH7017_TV_POWER_DOWN_EN);

	if (mode == DRM_MODE_DPMS_ON) {
		/* Turn on the LVDS */
		ch7017_write(dvo, CH7017_LVDS_POWER_DOWN,
			     val & ~CH7017_LVDS_POWER_DOWN_EN);
	} else {
		/* Turn off the LVDS */
		ch7017_write(dvo, CH7017_LVDS_POWER_DOWN,
			     val | CH7017_LVDS_POWER_DOWN_EN);
	}

	/* XXX: Should actually wait for update power status somehow */
	udelay(20000);
}

static void ch7017_dump_regs(struct intel_dvo_device *dvo)
{
	uint8_t val;

#define DUMP(reg)					\
do {							\
	ch7017_read(dvo, reg, &val);			\
	DRM_DEBUG(#reg ": %02x\n", val);		\
} while (0)

	DUMP(CH7017_HORIZONTAL_ACTIVE_PIXEL_INPUT);
	DUMP(CH7017_HORIZONTAL_ACTIVE_PIXEL_OUTPUT);
	DUMP(CH7017_VERTICAL_ACTIVE_LINE_OUTPUT);
	DUMP(CH7017_ACTIVE_INPUT_LINE_OUTPUT);
	DUMP(CH7017_LVDS_PLL_VCO_CONTROL);
	DUMP(CH7017_LVDS_PLL_FEEDBACK_DIV);
	DUMP(CH7017_LVDS_CONTROL_2);
	DUMP(CH7017_OUTPUTS_ENABLE);
	DUMP(CH7017_LVDS_POWER_DOWN);
}

static void ch7017_save(struct intel_dvo_device *dvo)
{
	struct ch7017_priv *priv = dvo->dev_priv;

	ch7017_read(dvo, CH7017_HORIZONTAL_ACTIVE_PIXEL_INPUT, &priv->save_hapi);
	ch7017_read(dvo, CH7017_VERTICAL_ACTIVE_LINE_OUTPUT, &priv->save_valo);
	ch7017_read(dvo, CH7017_ACTIVE_INPUT_LINE_OUTPUT, &priv->save_ailo);
	ch7017_read(dvo, CH7017_LVDS_PLL_VCO_CONTROL, &priv->save_lvds_pll_vco);
	ch7017_read(dvo, CH7017_LVDS_PLL_FEEDBACK_DIV, &priv->save_feedback_div);
	ch7017_read(dvo, CH7017_LVDS_CONTROL_2, &priv->save_lvds_control_2);
	ch7017_read(dvo, CH7017_OUTPUTS_ENABLE, &priv->save_outputs_enable);
	ch7017_read(dvo, CH7017_LVDS_POWER_DOWN, &priv->save_lvds_power_down);
	ch7017_read(dvo, CH7017_POWER_MANAGEMENT, &priv->save_power_management);
}

static void ch7017_restore(struct intel_dvo_device *dvo)
{
	struct ch7017_priv *priv = dvo->dev_priv;

	/* Power down before changing mode */
	ch7017_dpms(dvo, DRM_MODE_DPMS_OFF);

	ch7017_write(dvo, CH7017_HORIZONTAL_ACTIVE_PIXEL_INPUT, priv->save_hapi);
	ch7017_write(dvo, CH7017_VERTICAL_ACTIVE_LINE_OUTPUT, priv->save_valo);
	ch7017_write(dvo, CH7017_ACTIVE_INPUT_LINE_OUTPUT, priv->save_ailo);
	ch7017_write(dvo, CH7017_LVDS_PLL_VCO_CONTROL, priv->save_lvds_pll_vco);
	ch7017_write(dvo, CH7017_LVDS_PLL_FEEDBACK_DIV, priv->save_feedback_div);
	ch7017_write(dvo, CH7017_LVDS_CONTROL_2, priv->save_lvds_control_2);
	ch7017_write(dvo, CH7017_OUTPUTS_ENABLE, priv->save_outputs_enable);
	ch7017_write(dvo, CH7017_LVDS_POWER_DOWN, priv->save_lvds_power_down);
	ch7017_write(dvo, CH7017_POWER_MANAGEMENT, priv->save_power_management);
}

static void ch7017_destroy(struct intel_dvo_device *dvo)
{
	struct ch7017_priv *priv = dvo->dev_priv;

	if (priv) {
		kfree(priv);
		dvo->dev_priv = NULL;
	}
}

struct intel_dvo_dev_ops ch7017_ops = {
	.init = ch7017_init,
	.detect = ch7017_detect,
	.mode_valid = ch7017_mode_valid,
	.mode_set = ch7017_mode_set,
	.dpms = ch7017_dpms,
	.dump_regs = ch7017_dump_regs,
	.save = ch7017_save,
	.restore = ch7017_restore,
	.destroy = ch7017_destroy,
};