

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-common.h>
#include <media/soc_camera.h>
#include <media/tw9910.h>

#define GET_ID(val)  ((val & 0xF8) >> 3)
#define GET_ReV(val) (val & 0x07)

#define ID		0x00 /* Product ID Code Register */
#define STATUS1		0x01 /* Chip Status Register I */
#define INFORM		0x02 /* Input Format */
#define OPFORM		0x03 /* Output Format Control Register */
#define DLYCTR		0x04 /* Hysteresis and HSYNC Delay Control */
#define OUTCTR1		0x05 /* Output Control I */
#define ACNTL1		0x06 /* Analog Control Register 1 */
#define CROP_HI		0x07 /* Cropping Register, High */
#define VDELAY_LO	0x08 /* Vertical Delay Register, Low */
#define VACTIVE_LO	0x09 /* Vertical Active Register, Low */
#define HDELAY_LO	0x0A /* Horizontal Delay Register, Low */
#define HACTIVE_LO	0x0B /* Horizontal Active Register, Low */
#define CNTRL1		0x0C /* Control Register I */
#define VSCALE_LO	0x0D /* Vertical Scaling Register, Low */
#define SCALE_HI	0x0E /* Scaling Register, High */
#define HSCALE_LO	0x0F /* Horizontal Scaling Register, Low */
#define BRIGHT		0x10 /* BRIGHTNESS Control Register */
#define CONTRAST	0x11 /* CONTRAST Control Register */
#define SHARPNESS	0x12 /* SHARPNESS Control Register I */
#define SAT_U		0x13 /* Chroma (U) Gain Register */
#define SAT_V		0x14 /* Chroma (V) Gain Register */
#define HUE		0x15 /* Hue Control Register */
#define CORING1		0x17
#define CORING2		0x18 /* Coring and IF compensation */
#define VBICNTL		0x19 /* VBI Control Register */
#define ACNTL2		0x1A /* Analog Control 2 */
#define OUTCTR2		0x1B /* Output Control 2 */
#define SDT		0x1C /* Standard Selection */
#define SDTR		0x1D /* Standard Recognition */
#define TEST		0x1F /* Test Control Register */
#define CLMPG		0x20 /* Clamping Gain */
#define IAGC		0x21 /* Individual AGC Gain */
#define AGCGAIN		0x22 /* AGC Gain */
#define PEAKWT		0x23 /* White Peak Threshold */
#define CLMPL		0x24 /* Clamp level */
#define SYNCT		0x25 /* Sync Amplitude */
#define MISSCNT		0x26 /* Sync Miss Count Register */
#define PCLAMP		0x27 /* Clamp Position Register */
#define VCNTL1		0x28 /* Vertical Control I */
#define VCNTL2		0x29 /* Vertical Control II */
#define CKILL		0x2A /* Color Killer Level Control */
#define COMB		0x2B /* Comb Filter Control */
#define LDLY		0x2C /* Luma Delay and H Filter Control */
#define MISC1		0x2D /* Miscellaneous Control I */
#define LOOP		0x2E /* LOOP Control Register */
#define MISC2		0x2F /* Miscellaneous Control II */
#define MVSN		0x30 /* Macrovision Detection */
#define STATUS2		0x31 /* Chip STATUS II */
#define HFREF		0x32 /* H monitor */
#define CLMD		0x33 /* CLAMP MODE */
#define IDCNTL		0x34 /* ID Detection Control */
#define CLCNTL1		0x35 /* Clamp Control I */
#define ANAPLLCTL	0x4C
#define VBIMIN		0x4D
#define HSLOWCTL	0x4E
#define WSS3		0x4F
#define FILLDATA	0x50
#define SDID		0x51
#define DID		0x52
#define WSS1		0x53
#define WSS2		0x54
#define VVBI		0x55
#define LCTL6		0x56
#define LCTL7		0x57
#define LCTL8		0x58
#define LCTL9		0x59
#define LCTL10		0x5A
#define LCTL11		0x5B
#define LCTL12		0x5C
#define LCTL13		0x5D
#define LCTL14		0x5E
#define LCTL15		0x5F
#define LCTL16		0x60
#define LCTL17		0x61
#define LCTL18		0x62
#define LCTL19		0x63
#define LCTL20		0x64
#define LCTL21		0x65
#define LCTL22		0x66
#define LCTL23		0x67
#define LCTL24		0x68
#define LCTL25		0x69
#define LCTL26		0x6A
#define HSGEGIN		0x6B
#define HSEND		0x6C
#define OVSDLY		0x6D
#define OVSEND		0x6E
#define VBIDELAY	0x6F


/* INFORM */
#define FC27_ON     0x40 /* 1 : Input crystal clock frequency is 27MHz */
#define FC27_FF     0x00 /* 0 : Square pixel mode. */
			 /*     Must use 24.54MHz for 60Hz field rate */
			 /*     source or 29.5MHz for 50Hz field rate */
#define IFSEL_S     0x10 /* 01 : S-video decoding */
#define IFSEL_C     0x00 /* 00 : Composite video decoding */
			 /* Y input video selection */
#define YSEL_M0     0x00 /*  00 : Mux0 selected */
#define YSEL_M1     0x04 /*  01 : Mux1 selected */
#define YSEL_M2     0x08 /*  10 : Mux2 selected */
#define YSEL_M3     0x10 /*  11 : Mux3 selected */

/* OPFORM */
#define MODE        0x80 /* 0 : CCIR601 compatible YCrCb 4:2:2 format */
			 /* 1 : ITU-R-656 compatible data sequence format */
#define LEN         0x40 /* 0 : 8-bit YCrCb 4:2:2 output format */
			 /* 1 : 16-bit YCrCb 4:2:2 output format.*/
#define LLCMODE     0x20 /* 1 : LLC output mode. */
			 /* 0 : free-run output mode */
#define AINC        0x10 /* Serial interface auto-indexing control */
			 /* 0 : auto-increment */
			 /* 1 : non-auto */
#define VSCTL       0x08 /* 1 : Vertical out ctrl by DVALID */
			 /* 0 : Vertical out ctrl by HACTIVE and DVALID */
#define OEN         0x04 /* Output Enable together with TRI_SEL. */

/* OUTCTR1 */
#define VSP_LO      0x00 /* 0 : VS pin output polarity is active low */
#define VSP_HI      0x80 /* 1 : VS pin output polarity is active high. */
			 /* VS pin output control */
#define VSSL_VSYNC  0x00 /*   0 : VSYNC  */
#define VSSL_VACT   0x10 /*   1 : VACT   */
#define VSSL_FIELD  0x20 /*   2 : FIELD  */
#define VSSL_VVALID 0x30 /*   3 : VVALID */
#define VSSL_ZERO   0x70 /*   7 : 0      */
#define HSP_LOW     0x00 /* 0 : HS pin output polarity is active low */
#define HSP_HI      0x08 /* 1 : HS pin output polarity is active high.*/
			 /* HS pin output control */
#define HSSL_HACT   0x00 /*   0 : HACT   */
#define HSSL_HSYNC  0x01 /*   1 : HSYNC  */
#define HSSL_DVALID 0x02 /*   2 : DVALID */
#define HSSL_HLOCK  0x03 /*   3 : HLOCK  */
#define HSSL_ASYNCW 0x04 /*   4 : ASYNCW */
#define HSSL_ZERO   0x07 /*   7 : 0      */

/* ACNTL1 */
#define SRESET      0x80 /* resets the device to its default state
			  * but all register content remain unchanged.
			  * This bit is self-resetting.
			  */

/* VBICNTL */
#define RTSEL_MASK  0x07
#define RTSEL_VLOSS 0x00 /* 0000 = Video loss */
#define RTSEL_HLOCK 0x01 /* 0001 = H-lock */
#define RTSEL_SLOCK 0x02 /* 0010 = S-lock */
#define RTSEL_VLOCK 0x03 /* 0011 = V-lock */
#define RTSEL_MONO  0x04 /* 0100 = MONO */
#define RTSEL_DET50 0x05 /* 0101 = DET50 */
#define RTSEL_FIELD 0x06 /* 0110 = FIELD */
#define RTSEL_RTCO  0x07 /* 0111 = RTCO ( Real Time Control ) */


struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

struct tw9910_scale_ctrl {
	char           *name;
	unsigned short  width;
	unsigned short  height;
	u16             hscale;
	u16             vscale;
};

struct tw9910_cropping_ctrl {
	u16 vdelay;
	u16 vactive;
	u16 hdelay;
	u16 hactive;
};

struct tw9910_hsync_ctrl {
	u16 start;
	u16 end;
};

struct tw9910_priv {
	struct tw9910_video_info       *info;
	struct i2c_client              *client;
	struct soc_camera_device        icd;
	const struct tw9910_scale_ctrl *scale;
};


#define ENDMARKER { 0xff, 0xff }

static const struct regval_list tw9910_default_regs[] =
{
	{ OPFORM,  0x00 },
	{ OUTCTR1, VSP_LO | VSSL_VVALID | HSP_HI | HSSL_HSYNC },
	ENDMARKER,
};

static const struct soc_camera_data_format tw9910_color_fmt[] = {
	{
		.name       = "VYUY",
		.fourcc     = V4L2_PIX_FMT_VYUY,
		.depth      = 16,
		.colorspace = V4L2_COLORSPACE_SMPTE170M,
	}
};

static const struct tw9910_scale_ctrl tw9910_ntsc_scales[] = {
	{
		.name   = "NTSC SQ",
		.width  = 640,
		.height = 480,
		.hscale = 0x0100,
		.vscale = 0x0100,
	},
	{
		.name   = "NTSC CCIR601",
		.width  = 720,
		.height = 480,
		.hscale = 0x0100,
		.vscale = 0x0100,
	},
	{
		.name   = "NTSC SQ (CIF)",
		.width  = 320,
		.height = 240,
		.hscale = 0x0200,
		.vscale = 0x0200,
	},
	{
		.name   = "NTSC CCIR601 (CIF)",
		.width  = 360,
		.height = 240,
		.hscale = 0x0200,
		.vscale = 0x0200,
	},
	{
		.name   = "NTSC SQ (QCIF)",
		.width  = 160,
		.height = 120,
		.hscale = 0x0400,
		.vscale = 0x0400,
	},
	{
		.name   = "NTSC CCIR601 (QCIF)",
		.width  = 180,
		.height = 120,
		.hscale = 0x0400,
		.vscale = 0x0400,
	},
};

static const struct tw9910_scale_ctrl tw9910_pal_scales[] = {
	{
		.name   = "PAL SQ",
		.width  = 768,
		.height = 576,
		.hscale = 0x0100,
		.vscale = 0x0100,
	},
	{
		.name   = "PAL CCIR601",
		.width  = 720,
		.height = 576,
		.hscale = 0x0100,
		.vscale = 0x0100,
	},
	{
		.name   = "PAL SQ (CIF)",
		.width  = 384,
		.height = 288,
		.hscale = 0x0200,
		.vscale = 0x0200,
	},
	{
		.name   = "PAL CCIR601 (CIF)",
		.width  = 360,
		.height = 288,
		.hscale = 0x0200,
		.vscale = 0x0200,
	},
	{
		.name   = "PAL SQ (QCIF)",
		.width  = 192,
		.height = 144,
		.hscale = 0x0400,
		.vscale = 0x0400,
	},
	{
		.name   = "PAL CCIR601 (QCIF)",
		.width  = 180,
		.height = 144,
		.hscale = 0x0400,
		.vscale = 0x0400,
	},
};

static const struct tw9910_cropping_ctrl tw9910_cropping_ctrl = {
	.vdelay  = 0x0012,
	.vactive = 0x00F0,
	.hdelay  = 0x0010,
	.hactive = 0x02D0,
};

static const struct tw9910_hsync_ctrl tw9910_hsync_ctrl = {
	.start = 0x0260,
	.end   = 0x0300,
};

static int tw9910_set_scale(struct i2c_client *client,
			    const struct tw9910_scale_ctrl *scale)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, SCALE_HI,
					(scale->vscale & 0x0F00) >> 4 |
					(scale->hscale & 0x0F00) >> 8);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_write_byte_data(client, HSCALE_LO,
					scale->hscale & 0x00FF);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_write_byte_data(client, VSCALE_LO,
					scale->vscale & 0x00FF);

	return ret;
}

static int tw9910_set_cropping(struct i2c_client *client,
			       const struct tw9910_cropping_ctrl *cropping)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, CROP_HI,
					(cropping->vdelay  & 0x0300) >> 2 |
					(cropping->vactive & 0x0300) >> 4 |
					(cropping->hdelay  & 0x0300) >> 6 |
					(cropping->hactive & 0x0300) >> 8);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_write_byte_data(client, VDELAY_LO,
					cropping->vdelay & 0x00FF);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_write_byte_data(client, VACTIVE_LO,
					cropping->vactive & 0x00FF);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_write_byte_data(client, HDELAY_LO,
					cropping->hdelay & 0x00FF);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_write_byte_data(client, HACTIVE_LO,
					cropping->hactive & 0x00FF);

	return ret;
}

static int tw9910_set_hsync(struct i2c_client *client,
			    const struct tw9910_hsync_ctrl *hsync)
{
	int ret;

	/* bit 10 - 3 */
	ret = i2c_smbus_write_byte_data(client, HSGEGIN,
					(hsync->start & 0x07F8) >> 3);
	if (ret < 0)
		return ret;

	/* bit 10 - 3 */
	ret = i2c_smbus_write_byte_data(client, HSEND,
					(hsync->end & 0x07F8) >> 3);
	if (ret < 0)
		return ret;

	/* bit 2 - 0 */
	ret = i2c_smbus_read_byte_data(client, HSLOWCTL);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_write_byte_data(client, HSLOWCTL,
					(ret & 0x88)                 |
					(hsync->start & 0x0007) << 4 |
					(hsync->end   & 0x0007));

	return ret;
}

static int tw9910_write_array(struct i2c_client *client,
			      const struct regval_list *vals)
{
	while (vals->reg_num != 0xff) {
		int ret = i2c_smbus_write_byte_data(client,
						    vals->reg_num,
						    vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int tw9910_mask_set(struct i2c_client *client, u8 command,
			   u8 mask, u8 set)
{
	s32 val = i2c_smbus_read_byte_data(client, command);

	val &= ~mask;
	val |=  set;

	return i2c_smbus_write_byte_data(client, command, val);
}

static void tw9910_reset(struct i2c_client *client)
{
	i2c_smbus_write_byte_data(client, ACNTL1, SRESET);
	msleep(1);
}

static const struct tw9910_scale_ctrl*
tw9910_select_norm(struct soc_camera_device *icd, u32 width, u32 height)
{
	const struct tw9910_scale_ctrl *scale;
	const struct tw9910_scale_ctrl *ret = NULL;
	v4l2_std_id norm = icd->vdev->current_norm;
	__u32 diff = 0xffffffff, tmp;
	int size, i;

	if (norm & V4L2_STD_NTSC) {
		scale = tw9910_ntsc_scales;
		size = ARRAY_SIZE(tw9910_ntsc_scales);
	} else if (norm & V4L2_STD_PAL) {
		scale = tw9910_pal_scales;
		size = ARRAY_SIZE(tw9910_pal_scales);
	} else {
		return NULL;
	}

	for (i = 0; i < size; i++) {
		tmp = abs(width - scale[i].width) +
			abs(height - scale[i].height);
		if (tmp < diff) {
			diff = tmp;
			ret = scale + i;
		}
	}

	return ret;
}

static int tw9910_init(struct soc_camera_device *icd)
{
	struct tw9910_priv *priv = container_of(icd, struct tw9910_priv, icd);
	int ret = 0;

	if (priv->info->link.power) {
		ret = priv->info->link.power(&priv->client->dev, 1);
		if (ret < 0)
			return ret;
	}

	if (priv->info->link.reset)
		ret = priv->info->link.reset(&priv->client->dev);

	return ret;
}

static int tw9910_release(struct soc_camera_device *icd)
{
	struct tw9910_priv *priv = container_of(icd, struct tw9910_priv, icd);
	int ret = 0;

	if (priv->info->link.power)
		ret = priv->info->link.power(&priv->client->dev, 0);

	return ret;
}

static int tw9910_start_capture(struct soc_camera_device *icd)
{
	struct tw9910_priv *priv = container_of(icd, struct tw9910_priv, icd);

	if (!priv->scale) {
		dev_err(&icd->dev, "norm select error\n");
		return -EPERM;
	}

	dev_dbg(&icd->dev, "%s %dx%d\n",
		 priv->scale->name,
		 priv->scale->width,
		 priv->scale->height);

	return 0;
}

static int tw9910_stop_capture(struct soc_camera_device *icd)
{
	return 0;
}

static int tw9910_set_bus_param(struct soc_camera_device *icd,
				unsigned long flags)
{
	return 0;
}

static unsigned long tw9910_query_bus_param(struct soc_camera_device *icd)
{
	struct tw9910_priv *priv = container_of(icd, struct tw9910_priv, icd);
	struct soc_camera_link *icl = priv->client->dev.platform_data;
	unsigned long flags = SOCAM_PCLK_SAMPLE_RISING | SOCAM_MASTER |
		SOCAM_VSYNC_ACTIVE_HIGH | SOCAM_HSYNC_ACTIVE_HIGH |
		SOCAM_DATA_ACTIVE_HIGH | priv->info->buswidth;

	return soc_camera_apply_sensor_flags(icl, flags);
}

static int tw9910_get_chip_id(struct soc_camera_device *icd,
			      struct v4l2_dbg_chip_ident *id)
{
	id->ident = V4L2_IDENT_TW9910;
	id->revision = 0;

	return 0;
}

static int tw9910_set_std(struct soc_camera_device *icd,
			  v4l2_std_id *a)
{
	int ret = -EINVAL;

	if (*a & (V4L2_STD_NTSC | V4L2_STD_PAL))
		ret = 0;

	return ret;
}

static int tw9910_enum_input(struct soc_camera_device *icd,
			     struct v4l2_input *inp)
{
	inp->type = V4L2_INPUT_TYPE_TUNER;
	inp->std  = V4L2_STD_UNKNOWN;
	strcpy(inp->name, "Video");

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int tw9910_get_register(struct soc_camera_device *icd,
			       struct v4l2_dbg_register *reg)
{
	struct tw9910_priv *priv = container_of(icd, struct tw9910_priv, icd);
	int ret;

	if (reg->reg > 0xff)
		return -EINVAL;

	ret = i2c_smbus_read_byte_data(priv->client, reg->reg);
	if (ret < 0)
		return ret;

	/* ret      = int
	 * reg->val = __u64
	 */
	reg->val = (__u64)ret;

	return 0;
}

static int tw9910_set_register(struct soc_camera_device *icd,
			       struct v4l2_dbg_register *reg)
{
	struct tw9910_priv *priv = container_of(icd, struct tw9910_priv, icd);

	if (reg->reg > 0xff ||
	    reg->val > 0xff)
		return -EINVAL;

	return i2c_smbus_write_byte_data(priv->client, reg->reg, reg->val);
}
#endif

static int tw9910_set_fmt(struct soc_camera_device *icd, __u32 pixfmt,
			      struct v4l2_rect *rect)
{
	struct tw9910_priv *priv = container_of(icd, struct tw9910_priv, icd);
	int                 ret  = -EINVAL;
	u8                  val;

	/*
	 * select suitable norm
	 */
	priv->scale = tw9910_select_norm(icd, rect->width, rect->height);
	if (!priv->scale)
		goto tw9910_set_fmt_error;

	/*
	 * reset hardware
	 */
	tw9910_reset(priv->client);
	ret = tw9910_write_array(priv->client, tw9910_default_regs);
	if (ret < 0)
		goto tw9910_set_fmt_error;

	/*
	 * set bus width
	 */
	val = 0x00;
	if (SOCAM_DATAWIDTH_16 == priv->info->buswidth)
		val = LEN;

	ret = tw9910_mask_set(priv->client, OPFORM, LEN, val);
	if (ret < 0)
		goto tw9910_set_fmt_error;

	/*
	 * select MPOUT behavior
	 */
	switch (priv->info->mpout) {
	case TW9910_MPO_VLOSS:
		val = RTSEL_VLOSS; break;
	case TW9910_MPO_HLOCK:
		val = RTSEL_HLOCK; break;
	case TW9910_MPO_SLOCK:
		val = RTSEL_SLOCK; break;
	case TW9910_MPO_VLOCK:
		val = RTSEL_VLOCK; break;
	case TW9910_MPO_MONO:
		val = RTSEL_MONO;  break;
	case TW9910_MPO_DET50:
		val = RTSEL_DET50; break;
	case TW9910_MPO_FIELD:
		val = RTSEL_FIELD; break;
	case TW9910_MPO_RTCO:
		val = RTSEL_RTCO;  break;
	default:
		val = 0;
	}

	ret = tw9910_mask_set(priv->client, VBICNTL, RTSEL_MASK, val);
	if (ret < 0)
		goto tw9910_set_fmt_error;

	/*
	 * set scale
	 */
	ret = tw9910_set_scale(priv->client, priv->scale);
	if (ret < 0)
		goto tw9910_set_fmt_error;

	/*
	 * set cropping
	 */
	ret = tw9910_set_cropping(priv->client, &tw9910_cropping_ctrl);
	if (ret < 0)
		goto tw9910_set_fmt_error;

	/*
	 * set hsync
	 */
	ret = tw9910_set_hsync(priv->client, &tw9910_hsync_ctrl);
	if (ret < 0)
		goto tw9910_set_fmt_error;

	return ret;

tw9910_set_fmt_error:

	tw9910_reset(priv->client);
	priv->scale = NULL;

	return ret;
}

static int tw9910_try_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
	struct v4l2_pix_format *pix = &f->fmt.pix;
	const struct tw9910_scale_ctrl *scale;

	if (V4L2_FIELD_ANY == pix->field) {
		pix->field = V4L2_FIELD_INTERLACED;
	} else if (V4L2_FIELD_INTERLACED != pix->field) {
		dev_err(&icd->dev, "Field type invalid.\n");
		return -EINVAL;
	}

	/*
	 * select suitable norm
	 */
	scale = tw9910_select_norm(icd, pix->width, pix->height);
	if (!scale)
		return -EINVAL;

	pix->width  = scale->width;
	pix->height = scale->height;

	return 0;
}

static int tw9910_video_probe(struct soc_camera_device *icd)
{
	struct tw9910_priv *priv = container_of(icd, struct tw9910_priv, icd);
	s32 val;
	int ret;

	/*
	 * We must have a parent by now. And it cannot be a wrong one.
	 * So this entire test is completely redundant.
	 */
	if (!icd->dev.parent ||
	    to_soc_camera_host(icd->dev.parent)->nr != icd->iface)
		return -ENODEV;

	/*
	 * tw9910 only use 8 or 16 bit bus width
	 */
	if (SOCAM_DATAWIDTH_16 != priv->info->buswidth &&
	    SOCAM_DATAWIDTH_8  != priv->info->buswidth) {
		dev_err(&icd->dev, "bus width error\n");
		return -ENODEV;
	}

	icd->formats     = tw9910_color_fmt;
	icd->num_formats = ARRAY_SIZE(tw9910_color_fmt);

	/*
	 * check and show Product ID
	 */
	val = i2c_smbus_read_byte_data(priv->client, ID);
	if (0x0B != GET_ID(val) ||
	    0x00 != GET_ReV(val)) {
		dev_err(&icd->dev,
			"Product ID error %x:%x\n", GET_ID(val), GET_ReV(val));
		return -ENODEV;
	}

	dev_info(&icd->dev,
		 "tw9910 Product ID %0x:%0x\n", GET_ID(val), GET_ReV(val));

	ret = soc_camera_video_start(icd);
	if (ret < 0)
		return ret;

	icd->vdev->tvnorms      = V4L2_STD_NTSC | V4L2_STD_PAL;
	icd->vdev->current_norm = V4L2_STD_NTSC;

	return ret;
}

static void tw9910_video_remove(struct soc_camera_device *icd)
{
	soc_camera_video_stop(icd);
}

static struct soc_camera_ops tw9910_ops = {
	.owner			= THIS_MODULE,
	.probe			= tw9910_video_probe,
	.remove			= tw9910_video_remove,
	.init			= tw9910_init,
	.release		= tw9910_release,
	.start_capture		= tw9910_start_capture,
	.stop_capture		= tw9910_stop_capture,
	.set_fmt		= tw9910_set_fmt,
	.try_fmt		= tw9910_try_fmt,
	.set_bus_param		= tw9910_set_bus_param,
	.query_bus_param	= tw9910_query_bus_param,
	.get_chip_id		= tw9910_get_chip_id,
	.set_std		= tw9910_set_std,
	.enum_input		= tw9910_enum_input,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.get_register		= tw9910_get_register,
	.set_register		= tw9910_set_register,
#endif
};


static int tw9910_probe(struct i2c_client *client,
			const struct i2c_device_id *did)

{
	struct tw9910_priv             *priv;
	struct tw9910_video_info       *info;
	struct soc_camera_device       *icd;
	const struct tw9910_scale_ctrl *scale;
	int                             i, ret;

	info = client->dev.platform_data;
	if (!info)
		return -EINVAL;

	if (!i2c_check_functionality(to_i2c_adapter(client->dev.parent),
				     I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
			"I2C-Adapter doesn't support "
			"I2C_FUNC_SMBUS_BYTE_DATA\n");
		return -EIO;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->info   = info;
	priv->client = client;
	i2c_set_clientdata(client, priv);

	icd          = &priv->icd;
	icd->ops     = &tw9910_ops;
	icd->control = &client->dev;
	icd->iface   = info->link.bus_id;

	/*
	 * set width and height
	 */
	icd->width_max  = tw9910_ntsc_scales[0].width; /* set default */
	icd->width_min  = tw9910_ntsc_scales[0].width;
	icd->height_max = tw9910_ntsc_scales[0].height;
	icd->height_min = tw9910_ntsc_scales[0].height;

	scale = tw9910_ntsc_scales;
	for (i = 0; i < ARRAY_SIZE(tw9910_ntsc_scales); i++) {
		icd->width_max  = max(scale[i].width,  icd->width_max);
		icd->width_min  = min(scale[i].width,  icd->width_min);
		icd->height_max = max(scale[i].height, icd->height_max);
		icd->height_min = min(scale[i].height, icd->height_min);
	}
	scale = tw9910_pal_scales;
	for (i = 0; i < ARRAY_SIZE(tw9910_pal_scales); i++) {
		icd->width_max  = max(scale[i].width,  icd->width_max);
		icd->width_min  = min(scale[i].width,  icd->width_min);
		icd->height_max = max(scale[i].height, icd->height_max);
		icd->height_min = min(scale[i].height, icd->height_min);
	}

	ret = soc_camera_device_register(icd);

	if (ret) {
		i2c_set_clientdata(client, NULL);
		kfree(priv);
	}

	return ret;
}

static int tw9910_remove(struct i2c_client *client)
{
	struct tw9910_priv *priv = i2c_get_clientdata(client);

	soc_camera_device_unregister(&priv->icd);
	i2c_set_clientdata(client, NULL);
	kfree(priv);
	return 0;
}

static const struct i2c_device_id tw9910_id[] = {
	{ "tw9910", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tw9910_id);

static struct i2c_driver tw9910_i2c_driver = {
	.driver = {
		.name = "tw9910",
	},
	.probe    = tw9910_probe,
	.remove   = tw9910_remove,
	.id_table = tw9910_id,
};

static int __init tw9910_module_init(void)
{
	return i2c_add_driver(&tw9910_i2c_driver);
}

static void __exit tw9910_module_exit(void)
{
	i2c_del_driver(&tw9910_i2c_driver);
}

module_init(tw9910_module_init);
module_exit(tw9910_module_exit);

MODULE_DESCRIPTION("SoC Camera driver for tw9910");
MODULE_AUTHOR("Kuninori Morimoto");
MODULE_LICENSE("GPL v2");
