

#ifndef _CX25840_H_
#define _CX25840_H_

enum cx25840_video_input {
	/* Composite video inputs In1-In8 */
	CX25840_COMPOSITE1 = 1,
	CX25840_COMPOSITE2,
	CX25840_COMPOSITE3,
	CX25840_COMPOSITE4,
	CX25840_COMPOSITE5,
	CX25840_COMPOSITE6,
	CX25840_COMPOSITE7,
	CX25840_COMPOSITE8,

	/* S-Video inputs consist of one luma input (In1-In8) ORed with one
	   chroma input (In5-In8) */
	CX25840_SVIDEO_LUMA1 = 0x10,
	CX25840_SVIDEO_LUMA2 = 0x20,
	CX25840_SVIDEO_LUMA3 = 0x30,
	CX25840_SVIDEO_LUMA4 = 0x40,
	CX25840_SVIDEO_LUMA5 = 0x50,
	CX25840_SVIDEO_LUMA6 = 0x60,
	CX25840_SVIDEO_LUMA7 = 0x70,
	CX25840_SVIDEO_LUMA8 = 0x80,
	CX25840_SVIDEO_CHROMA4 = 0x400,
	CX25840_SVIDEO_CHROMA5 = 0x500,
	CX25840_SVIDEO_CHROMA6 = 0x600,
	CX25840_SVIDEO_CHROMA7 = 0x700,
	CX25840_SVIDEO_CHROMA8 = 0x800,

	/* S-Video aliases for common luma/chroma combinations */
	CX25840_SVIDEO1 = 0x510,
	CX25840_SVIDEO2 = 0x620,
	CX25840_SVIDEO3 = 0x730,
	CX25840_SVIDEO4 = 0x840,

	/* Allow frames to specify specific input configurations */
	CX25840_VIN1_CH1  = 0x80000000,
	CX25840_VIN2_CH1  = 0x80000001,
	CX25840_VIN3_CH1  = 0x80000002,
	CX25840_VIN4_CH1  = 0x80000003,
	CX25840_VIN5_CH1  = 0x80000004,
	CX25840_VIN6_CH1  = 0x80000005,
	CX25840_VIN7_CH1  = 0x80000006,
	CX25840_VIN8_CH1  = 0x80000007,
	CX25840_VIN4_CH2  = 0x80000000,
	CX25840_VIN5_CH2  = 0x80000010,
	CX25840_VIN6_CH2  = 0x80000020,
	CX25840_NONE_CH2  = 0x80000030,
	CX25840_VIN7_CH3  = 0x80000000,
	CX25840_VIN8_CH3  = 0x80000040,
	CX25840_NONE0_CH3 = 0x80000080,
	CX25840_NONE1_CH3 = 0x800000c0,
	CX25840_SVIDEO_ON = 0x80000100,
};

enum cx25840_audio_input {
	/* Audio inputs: serial or In4-In8 */
	CX25840_AUDIO_SERIAL,
	CX25840_AUDIO4 = 4,
	CX25840_AUDIO5,
	CX25840_AUDIO6,
	CX25840_AUDIO7,
	CX25840_AUDIO8,
};

#endif
