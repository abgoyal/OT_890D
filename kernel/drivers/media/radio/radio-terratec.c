

#include <linux/module.h>	/* Modules 			*/
#include <linux/init.h>		/* Initdata			*/
#include <linux/ioport.h>	/* request_region		*/
#include <linux/delay.h>	/* udelay			*/
#include <asm/io.h>		/* outb, outb_p			*/
#include <asm/uaccess.h>	/* copy to/from user		*/
#include <linux/videodev2.h>	/* kernel radio structs		*/
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <linux/spinlock.h>

#include <linux/version.h>      /* for KERNEL_VERSION MACRO     */
#define RADIO_VERSION KERNEL_VERSION(0,0,2)

static struct v4l2_queryctrl radio_qctrl[] = {
	{
		.id            = V4L2_CID_AUDIO_MUTE,
		.name          = "Mute",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	},{
		.id            = V4L2_CID_AUDIO_VOLUME,
		.name          = "Volume",
		.minimum       = 0,
		.maximum       = 0xff,
		.step          = 1,
		.default_value = 0xff,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	}
};

#ifndef CONFIG_RADIO_TERRATEC_PORT
#define CONFIG_RADIO_TERRATEC_PORT 0x590
#endif

/**************** this ones are for the terratec *******************/
#define BASEPORT 	0x590
#define VOLPORT 	0x591
#define WRT_DIS 	0x00
#define CLK_OFF		0x00
#define IIC_DATA	0x01
#define IIC_CLK		0x02
#define DATA		0x04
#define CLK_ON 		0x08
#define WRT_EN		0x10
/*******************************************************************/

static int io = CONFIG_RADIO_TERRATEC_PORT;
static int radio_nr = -1;
static spinlock_t lock;

struct tt_device
{
	unsigned long in_use;
	int port;
	int curvol;
	unsigned long curfreq;
	int muted;
};


/* local things */

static void cardWriteVol(int volume)
{
	int i;
	volume = volume+(volume * 32); // change both channels
	spin_lock(&lock);
	for (i=0;i<8;i++)
	{
		if (volume & (0x80>>i))
			outb(0x80, VOLPORT);
		else outb(0x00, VOLPORT);
	}
	spin_unlock(&lock);
}



static void tt_mute(struct tt_device *dev)
{
	dev->muted = 1;
	cardWriteVol(0);
}

static int tt_setvol(struct tt_device *dev, int vol)
{

//	printk(KERN_ERR "setvol called, vol = %d\n", vol);

	if(vol == dev->curvol) {	/* requested volume = current */
		if (dev->muted) {	/* user is unmuting the card  */
			dev->muted = 0;
			cardWriteVol(vol);	/* enable card */
		}

		return 0;
	}

	if(vol == 0) {			/* volume = 0 means mute the card */
		cardWriteVol(0);	/* "turn off card" by setting vol to 0 */
		dev->curvol = vol;	/* track the volume state!	*/
		return 0;
	}

	dev->muted = 0;

	cardWriteVol(vol);

	dev->curvol = vol;

	return 0;

}


/* this is the worst part in this driver */
/* many more or less strange things are going on here, but hey, it works :) */

static int tt_setfreq(struct tt_device *dev, unsigned long freq1)
{
	int freq;
	int i;
	int p;
	int  temp;
	long rest;

	unsigned char buffer[25];		/* we have to bit shift 25 registers */
	freq = freq1/160;			/* convert the freq. to a nice to handle value */
	for(i=24;i>-1;i--)
		buffer[i]=0;

	rest = freq*10+10700;		/* i once had understood what is going on here */
					/* maybe some wise guy (friedhelm?) can comment this stuff */
	i=13;
	p=10;
	temp=102400;
	while (rest!=0)
	{
		if (rest%temp  == rest)
			buffer[i] = 0;
		else
		{
			buffer[i] = 1;
			rest = rest-temp;
		}
		i--;
		p--;
		temp = temp/2;
	}

	spin_lock(&lock);

	for (i=24;i>-1;i--)			/* bit shift the values to the radiocard */
	{
		if (buffer[i]==1)
		{
			outb(WRT_EN|DATA, BASEPORT);
			outb(WRT_EN|DATA|CLK_ON  , BASEPORT);
			outb(WRT_EN|DATA, BASEPORT);
		}
		else
		{
			outb(WRT_EN|0x00, BASEPORT);
			outb(WRT_EN|0x00|CLK_ON  , BASEPORT);
		}
	}
	outb(0x00, BASEPORT);

	spin_unlock(&lock);

	return 0;
}

static int tt_getsigstr(struct tt_device *dev)		/* TODO */
{
	if (inb(io) & 2)	/* bit set = no signal present	*/
		return 0;
	return 1;		/* signal present		*/
}

static int vidioc_querycap(struct file *file, void *priv,
					struct v4l2_capability *v)
{
	strlcpy(v->driver, "radio-terratec", sizeof(v->driver));
	strlcpy(v->card, "ActiveRadio", sizeof(v->card));
	sprintf(v->bus_info, "ISA");
	v->version = RADIO_VERSION;
	v->capabilities = V4L2_CAP_TUNER;
	return 0;
}

static int vidioc_g_tuner(struct file *file, void *priv,
					struct v4l2_tuner *v)
{
	struct tt_device *tt = video_drvdata(file);

	if (v->index > 0)
		return -EINVAL;

	strcpy(v->name, "FM");
	v->type = V4L2_TUNER_RADIO;
	v->rangelow = (87*16000);
	v->rangehigh = (108*16000);
	v->rxsubchans = V4L2_TUNER_SUB_MONO;
	v->capability = V4L2_TUNER_CAP_LOW;
	v->audmode = V4L2_TUNER_MODE_MONO;
	v->signal = 0xFFFF*tt_getsigstr(tt);
	return 0;
}

static int vidioc_s_tuner(struct file *file, void *priv,
					struct v4l2_tuner *v)
{
	if (v->index > 0)
		return -EINVAL;
	return 0;
}

static int vidioc_s_frequency(struct file *file, void *priv,
					struct v4l2_frequency *f)
{
	struct tt_device *tt = video_drvdata(file);

	tt->curfreq = f->frequency;
	tt_setfreq(tt, tt->curfreq);
	return 0;
}

static int vidioc_g_frequency(struct file *file, void *priv,
					struct v4l2_frequency *f)
{
	struct tt_device *tt = video_drvdata(file);

	f->type = V4L2_TUNER_RADIO;
	f->frequency = tt->curfreq;
	return 0;
}

static int vidioc_queryctrl(struct file *file, void *priv,
					struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(radio_qctrl); i++) {
		if (qc->id && qc->id == radio_qctrl[i].id) {
			memcpy(qc, &(radio_qctrl[i]),
						sizeof(*qc));
			return 0;
		}
	}
	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
					struct v4l2_control *ctrl)
{
	struct tt_device *tt = video_drvdata(file);

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:
		if (tt->muted)
			ctrl->value = 1;
		else
			ctrl->value = 0;
		return 0;
	case V4L2_CID_AUDIO_VOLUME:
		ctrl->value = tt->curvol * 6554;
		return 0;
	}
	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
					struct v4l2_control *ctrl)
{
	struct tt_device *tt = video_drvdata(file);

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:
		if (ctrl->value)
			tt_mute(tt);
		else
			tt_setvol(tt,tt->curvol);
		return 0;
	case V4L2_CID_AUDIO_VOLUME:
		tt_setvol(tt,ctrl->value);
		return 0;
	}
	return -EINVAL;
}

static int vidioc_g_audio(struct file *file, void *priv,
					struct v4l2_audio *a)
{
	if (a->index > 1)
		return -EINVAL;

	strcpy(a->name, "Radio");
	a->capability = V4L2_AUDCAP_STEREO;
	return 0;
}

static int vidioc_g_input(struct file *filp, void *priv, unsigned int *i)
{
	*i = 0;
	return 0;
}

static int vidioc_s_input(struct file *filp, void *priv, unsigned int i)
{
	if (i != 0)
		return -EINVAL;
	return 0;
}

static int vidioc_s_audio(struct file *file, void *priv,
					struct v4l2_audio *a)
{
	if (a->index != 0)
		return -EINVAL;
	return 0;
}

static struct tt_device terratec_unit;

static int terratec_exclusive_open(struct file *file)
{
	return test_and_set_bit(0, &terratec_unit.in_use) ? -EBUSY : 0;
}

static int terratec_exclusive_release(struct file *file)
{
	clear_bit(0, &terratec_unit.in_use);
	return 0;
}

static const struct v4l2_file_operations terratec_fops = {
	.owner		= THIS_MODULE,
	.open           = terratec_exclusive_open,
	.release        = terratec_exclusive_release,
	.ioctl		= video_ioctl2,
};

static const struct v4l2_ioctl_ops terratec_ioctl_ops = {
	.vidioc_querycap    = vidioc_querycap,
	.vidioc_g_tuner     = vidioc_g_tuner,
	.vidioc_s_tuner     = vidioc_s_tuner,
	.vidioc_g_frequency = vidioc_g_frequency,
	.vidioc_s_frequency = vidioc_s_frequency,
	.vidioc_queryctrl   = vidioc_queryctrl,
	.vidioc_g_ctrl      = vidioc_g_ctrl,
	.vidioc_s_ctrl      = vidioc_s_ctrl,
	.vidioc_g_audio     = vidioc_g_audio,
	.vidioc_s_audio     = vidioc_s_audio,
	.vidioc_g_input     = vidioc_g_input,
	.vidioc_s_input     = vidioc_s_input,
};

static struct video_device terratec_radio = {
	.name		= "TerraTec ActiveRadio",
	.fops           = &terratec_fops,
	.ioctl_ops 	= &terratec_ioctl_ops,
	.release	= video_device_release_empty,
};

static int __init terratec_init(void)
{
	if(io==-1)
	{
		printk(KERN_ERR "You must set an I/O address with io=0x???\n");
		return -EINVAL;
	}
	if (!request_region(io, 2, "terratec"))
	{
		printk(KERN_ERR "TerraTec: port 0x%x already in use\n", io);
		return -EBUSY;
	}

	video_set_drvdata(&terratec_radio, &terratec_unit);

	spin_lock_init(&lock);

	if (video_register_device(&terratec_radio, VFL_TYPE_RADIO, radio_nr) < 0) {
		release_region(io,2);
		return -EINVAL;
	}

	printk(KERN_INFO "TERRATEC ActivRadio Standalone card driver.\n");

	/* mute card - prevents noisy bootups */

	/* this ensures that the volume is all the way down  */
	cardWriteVol(0);
	terratec_unit.curvol = 0;

	return 0;
}

MODULE_AUTHOR("R.OFFERMANNS & others");
MODULE_DESCRIPTION("A driver for the TerraTec ActiveRadio Standalone radio card.");
MODULE_LICENSE("GPL");
module_param(io, int, 0);
MODULE_PARM_DESC(io, "I/O address of the TerraTec ActiveRadio card (0x590 or 0x591)");
module_param(radio_nr, int, 0);

static void __exit terratec_cleanup_module(void)
{
	video_unregister_device(&terratec_radio);
	release_region(io,2);
	printk(KERN_INFO "TERRATEC ActivRadio Standalone card driver unloaded.\n");
}

module_init(terratec_init);
module_exit(terratec_cleanup_module);

