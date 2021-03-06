

static int stb6100_get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct dvb_frontend_ops	*frontend_ops = NULL;
	struct dvb_tuner_ops	*tuner_ops = NULL;
	struct tuner_state	t_state;
	int err = 0;

	if (&fe->ops)
		frontend_ops = &fe->ops;
	if (&frontend_ops->tuner_ops)
		tuner_ops = &frontend_ops->tuner_ops;
	if (tuner_ops->get_state) {
		if ((err = tuner_ops->get_state(fe, DVBFE_TUNER_FREQUENCY, &t_state)) < 0) {
			printk("%s: Invalid parameter\n", __func__);
			return err;
		}
		*frequency = t_state.frequency;
		printk("%s: Frequency=%d\n", __func__, t_state.frequency);
	}
	return 0;
}

static int stb6100_set_frequency(struct dvb_frontend *fe, u32 frequency)
{
	struct dvb_frontend_ops	*frontend_ops = NULL;
	struct dvb_tuner_ops	*tuner_ops = NULL;
	struct tuner_state	t_state;
	int err = 0;

	t_state.frequency = frequency;
	if (&fe->ops)
		frontend_ops = &fe->ops;
	if (&frontend_ops->tuner_ops)
		tuner_ops = &frontend_ops->tuner_ops;
	if (tuner_ops->set_state) {
		if ((err = tuner_ops->set_state(fe, DVBFE_TUNER_FREQUENCY, &t_state)) < 0) {
			printk("%s: Invalid parameter\n", __func__);
			return err;
		}
	}
	printk("%s: Frequency=%d\n", __func__, t_state.frequency);
	return 0;
}

static int stb6100_get_bandwidth(struct dvb_frontend *fe, u32 *bandwidth)
{
	struct dvb_frontend_ops	*frontend_ops = &fe->ops;
	struct dvb_tuner_ops	*tuner_ops = &frontend_ops->tuner_ops;
	struct tuner_state	t_state;
	int err = 0;

	if (&fe->ops)
		frontend_ops = &fe->ops;
	if (&frontend_ops->tuner_ops)
		tuner_ops = &frontend_ops->tuner_ops;
	if (tuner_ops->get_state) {
		if ((err = tuner_ops->get_state(fe, DVBFE_TUNER_BANDWIDTH, &t_state)) < 0) {
			printk("%s: Invalid parameter\n", __func__);
			return err;
		}
		*bandwidth = t_state.bandwidth;
	}
	printk("%s: Bandwidth=%d\n", __func__, t_state.bandwidth);
	return 0;
}

static int stb6100_set_bandwidth(struct dvb_frontend *fe, u32 bandwidth)
{
	struct dvb_frontend_ops	*frontend_ops = NULL;
	struct dvb_tuner_ops	*tuner_ops = NULL;
	struct tuner_state	t_state;
	int err = 0;

	t_state.bandwidth = bandwidth;
	if (&fe->ops)
		frontend_ops = &fe->ops;
	if (&frontend_ops->tuner_ops)
		tuner_ops = &frontend_ops->tuner_ops;
	if (tuner_ops->set_state) {
		if ((err = tuner_ops->set_state(fe, DVBFE_TUNER_BANDWIDTH, &t_state)) < 0) {
			printk("%s: Invalid parameter\n", __func__);
			return err;
		}
	}
	printk("%s: Bandwidth=%d\n", __func__, t_state.bandwidth);
	return 0;
}
