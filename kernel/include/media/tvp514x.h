

#ifndef _TVP514X_H
#define _TVP514X_H

#define TVP514X_MODULE_NAME		"tvp514x"

#define TVP514X_XCLK_BT656		(27000000)

/* Number of pixels and number of lines per frame for different standards */
#define NTSC_NUM_ACTIVE_PIXELS		(720)
#define NTSC_NUM_ACTIVE_LINES		(480)
#define PAL_NUM_ACTIVE_PIXELS		(720)
#define PAL_NUM_ACTIVE_LINES		(576)

enum tvp514x_input {
	/*
	 * CVBS input selection
	 */
	INPUT_CVBS_VI1A = 0x0,
	INPUT_CVBS_VI1B,
	INPUT_CVBS_VI1C,
	INPUT_CVBS_VI2A = 0x04,
	INPUT_CVBS_VI2B,
	INPUT_CVBS_VI2C,
	INPUT_CVBS_VI3A = 0x08,
	INPUT_CVBS_VI3B,
	INPUT_CVBS_VI3C,
	INPUT_CVBS_VI4A = 0x0C,
	/*
	 * S-Video input selection
	 */
	INPUT_SVIDEO_VI2A_VI1A = 0x44,
	INPUT_SVIDEO_VI2B_VI1B,
	INPUT_SVIDEO_VI2C_VI1C,
	INPUT_SVIDEO_VI2A_VI3A = 0x54,
	INPUT_SVIDEO_VI2B_VI3B,
	INPUT_SVIDEO_VI2C_VI3C,
	INPUT_SVIDEO_VI4A_VI1A = 0x4C,
	INPUT_SVIDEO_VI4A_VI1B,
	INPUT_SVIDEO_VI4A_VI1C,
	INPUT_SVIDEO_VI4A_VI3A = 0x5C,
	INPUT_SVIDEO_VI4A_VI3B,
	INPUT_SVIDEO_VI4A_VI3C,

	/* Need to add entries for
	 * RGB, YPbPr and SCART.
	 */
	INPUT_INVALID
};

enum tvp514x_output {
	OUTPUT_10BIT_422_EMBEDDED_SYNC = 0,
	OUTPUT_20BIT_422_SEPERATE_SYNC,
	OUTPUT_10BIT_422_SEPERATE_SYNC = 3,
	OUTPUT_INVALID
};

struct tvp514x_platform_data {
	char *master;
	int (*power_set) (enum v4l2_power on);
	int (*ifparm) (struct v4l2_ifparm *p);
	int (*priv_data_set) (void *);
	/* Interface control params */
	bool clk_polarity;
	bool hs_polarity;
	bool vs_polarity;
};


#endif				/* ifndef _TVP514X_H */
