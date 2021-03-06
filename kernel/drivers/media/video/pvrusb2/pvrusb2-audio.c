

#include "pvrusb2-audio.h"
#include "pvrusb2-hdw-internal.h"
#include "pvrusb2-debug.h"
#include <linux/videodev2.h>
#include <media/msp3400.h>
#include <media/v4l2-common.h>

struct pvr2_msp3400_handler {
	struct pvr2_hdw *hdw;
	struct pvr2_i2c_client *client;
	struct pvr2_i2c_handler i2c_handler;
	unsigned long stale_mask;
};



struct routing_scheme {
	const int *def;
	unsigned int cnt;
};

static const int routing_scheme0[] = {
	[PVR2_CVAL_INPUT_TV]        = MSP_INPUT_DEFAULT,
	[PVR2_CVAL_INPUT_RADIO]     = MSP_INPUT(MSP_IN_SCART2,
						MSP_IN_TUNER1,
						MSP_DSP_IN_SCART,
						MSP_DSP_IN_SCART),
	[PVR2_CVAL_INPUT_COMPOSITE] = MSP_INPUT(MSP_IN_SCART1,
						MSP_IN_TUNER1,
						MSP_DSP_IN_SCART,
						MSP_DSP_IN_SCART),
	[PVR2_CVAL_INPUT_SVIDEO]    = MSP_INPUT(MSP_IN_SCART1,
						MSP_IN_TUNER1,
						MSP_DSP_IN_SCART,
						MSP_DSP_IN_SCART),
};

static const struct routing_scheme routing_schemes[] = {
	[PVR2_ROUTING_SCHEME_HAUPPAUGE] = {
		.def = routing_scheme0,
		.cnt = ARRAY_SIZE(routing_scheme0),
	},
};

/* This function selects the correct audio input source */
static void set_stereo(struct pvr2_msp3400_handler *ctxt)
{
	struct pvr2_hdw *hdw = ctxt->hdw;
	struct v4l2_routing route;
	const struct routing_scheme *sp;
	unsigned int sid = hdw->hdw_desc->signal_routing_scheme;

	pvr2_trace(PVR2_TRACE_CHIPS,"i2c msp3400 v4l2 set_stereo");

	if ((sid < ARRAY_SIZE(routing_schemes)) &&
	    ((sp = routing_schemes + sid) != NULL) &&
	    (hdw->input_val >= 0) &&
	    (hdw->input_val < sp->cnt)) {
		route.input = sp->def[hdw->input_val];
	} else {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "*** WARNING *** i2c msp3400 v4l2 set_stereo:"
			   " Invalid routing scheme (%u) and/or input (%d)",
			   sid,hdw->input_val);
		return;
	}
	route.output = MSP_OUTPUT(MSP_SC_IN_DSP_SCART1);
	pvr2_i2c_client_cmd(ctxt->client,VIDIOC_INT_S_AUDIO_ROUTING,&route);
}


static int check_stereo(struct pvr2_msp3400_handler *ctxt)
{
	struct pvr2_hdw *hdw = ctxt->hdw;
	return hdw->input_dirty;
}


struct pvr2_msp3400_ops {
	void (*update)(struct pvr2_msp3400_handler *);
	int (*check)(struct pvr2_msp3400_handler *);
};


static const struct pvr2_msp3400_ops msp3400_ops[] = {
	{ .update = set_stereo, .check = check_stereo},
};


static int msp3400_check(struct pvr2_msp3400_handler *ctxt)
{
	unsigned long msk;
	unsigned int idx;

	for (idx = 0; idx < ARRAY_SIZE(msp3400_ops); idx++) {
		msk = 1 << idx;
		if (ctxt->stale_mask & msk) continue;
		if (msp3400_ops[idx].check(ctxt)) {
			ctxt->stale_mask |= msk;
		}
	}
	return ctxt->stale_mask != 0;
}


static void msp3400_update(struct pvr2_msp3400_handler *ctxt)
{
	unsigned long msk;
	unsigned int idx;

	for (idx = 0; idx < ARRAY_SIZE(msp3400_ops); idx++) {
		msk = 1 << idx;
		if (!(ctxt->stale_mask & msk)) continue;
		ctxt->stale_mask &= ~msk;
		msp3400_ops[idx].update(ctxt);
	}
}


static void pvr2_msp3400_detach(struct pvr2_msp3400_handler *ctxt)
{
	ctxt->client->handler = NULL;
	kfree(ctxt);
}


static unsigned int pvr2_msp3400_describe(struct pvr2_msp3400_handler *ctxt,
					  char *buf,unsigned int cnt)
{
	return scnprintf(buf,cnt,"handler: pvrusb2-audio v4l2");
}


static const struct pvr2_i2c_handler_functions msp3400_funcs = {
	.detach = (void (*)(void *))pvr2_msp3400_detach,
	.check = (int (*)(void *))msp3400_check,
	.update = (void (*)(void *))msp3400_update,
	.describe = (unsigned int (*)(void *,char *,unsigned int))pvr2_msp3400_describe,
};


int pvr2_i2c_msp3400_setup(struct pvr2_hdw *hdw,struct pvr2_i2c_client *cp)
{
	struct pvr2_msp3400_handler *ctxt;
	if (cp->handler) return 0;

	ctxt = kzalloc(sizeof(*ctxt),GFP_KERNEL);
	if (!ctxt) return 0;

	ctxt->i2c_handler.func_data = ctxt;
	ctxt->i2c_handler.func_table = &msp3400_funcs;
	ctxt->client = cp;
	ctxt->hdw = hdw;
	ctxt->stale_mask = (1 << ARRAY_SIZE(msp3400_ops)) - 1;
	cp->handler = &ctxt->i2c_handler;
	pvr2_trace(PVR2_TRACE_CHIPS,"i2c 0x%x msp3400 V4L2 handler set up",
		   cp->client->addr);
	return !0;
}


