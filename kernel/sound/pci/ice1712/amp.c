

#include <asm/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <sound/core.h>

#include "ice1712.h"
#include "envy24ht.h"
#include "amp.h"

static void wm_put(struct snd_ice1712 *ice, int reg, unsigned short val)
{
	unsigned short cval;
	cval = (reg << 9) | val;
	snd_vt1724_write_i2c(ice, WM_DEV, cval >> 8, cval & 0xff);
}

static int __devinit snd_vt1724_amp_init(struct snd_ice1712 *ice)
{
	static const unsigned short wm_inits[] = {
		WM_ATTEN_L,	0x0000,	/* 0 db */
		WM_ATTEN_R,	0x0000,	/* 0 db */
		WM_DAC_CTRL,	0x0008,	/* 24bit I2S */
		WM_INT_CTRL,	0x0001, /* 24bit I2S */	
	};

	unsigned int i;

	/* only use basic functionality for now */

	ice->num_total_dacs = 2;	/* only PSDOUT0 is connected */
	ice->num_total_adcs = 2;

	/* Chaintech AV-710 has another codecs, which need initialization */
	/* initialize WM8728 codec */
	if (ice->eeprom.subvendor == VT1724_SUBDEVICE_AV710) {
		for (i = 0; i < ARRAY_SIZE(wm_inits); i += 2)
			wm_put(ice, wm_inits[i], wm_inits[i+1]);
	}

	return 0;
}

static int __devinit snd_vt1724_amp_add_controls(struct snd_ice1712 *ice)
{
	/* we use pins 39 and 41 of the VT1616 for left and right read outputs */
	snd_ac97_write_cache(ice->ac97, 0x5a, snd_ac97_read(ice->ac97, 0x5a) & ~0x8000);
	return 0;
}


/* entry point */
struct snd_ice1712_card_info snd_vt1724_amp_cards[] __devinitdata = {
	{
		.subvendor = VT1724_SUBDEVICE_AV710,
		.name = "Chaintech AV-710",
		.model = "av710",
		.chip_init = snd_vt1724_amp_init,
		.build_controls = snd_vt1724_amp_add_controls,
	},
	{
		.subvendor = VT1724_SUBDEVICE_AUDIO2000,
		.name = "AMP Ltd AUDIO2000",
		.model = "amp2000",
		.chip_init = snd_vt1724_amp_init,
		.build_controls = snd_vt1724_amp_add_controls,
	},
	{ } /* terminator */
};

