

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include "mct_u232.h"

#define DRIVER_VERSION "z2.1"		/* Linux in-kernel version */
#define DRIVER_AUTHOR "Wolfgang Grandegger <wolfgang@ces.ch>"
#define DRIVER_DESC "Magic Control Technology USB-RS232 converter driver"

static int debug;

static int  mct_u232_startup(struct usb_serial *serial);
static void mct_u232_shutdown(struct usb_serial *serial);
static int  mct_u232_open(struct tty_struct *tty,
			struct usb_serial_port *port, struct file *filp);
static void mct_u232_close(struct tty_struct *tty,
			struct usb_serial_port *port, struct file *filp);
static void mct_u232_read_int_callback(struct urb *urb);
static void mct_u232_set_termios(struct tty_struct *tty,
			struct usb_serial_port *port, struct ktermios *old);
static void mct_u232_break_ctl(struct tty_struct *tty, int break_state);
static int  mct_u232_tiocmget(struct tty_struct *tty, struct file *file);
static int  mct_u232_tiocmset(struct tty_struct *tty, struct file *file,
			unsigned int set, unsigned int clear);
static void mct_u232_throttle(struct tty_struct *tty);
static void mct_u232_unthrottle(struct tty_struct *tty);


static struct usb_device_id id_table_combined [] = {
	{ USB_DEVICE(MCT_U232_VID, MCT_U232_PID) },
	{ USB_DEVICE(MCT_U232_VID, MCT_U232_SITECOM_PID) },
	{ USB_DEVICE(MCT_U232_VID, MCT_U232_DU_H3SP_PID) },
	{ USB_DEVICE(MCT_U232_BELKIN_F5U109_VID, MCT_U232_BELKIN_F5U109_PID) },
	{ }		/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, id_table_combined);

static struct usb_driver mct_u232_driver = {
	.name =		"mct_u232",
	.probe =	usb_serial_probe,
	.disconnect =	usb_serial_disconnect,
	.id_table =	id_table_combined,
	.no_dynamic_id = 	1,
};

static struct usb_serial_driver mct_u232_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"mct_u232",
	},
	.description =	     "MCT U232",
	.usb_driver = 	     &mct_u232_driver,
	.id_table =	     id_table_combined,
	.num_ports =	     1,
	.open =		     mct_u232_open,
	.close =	     mct_u232_close,
	.throttle =	     mct_u232_throttle,
	.unthrottle =	     mct_u232_unthrottle,
	.read_int_callback = mct_u232_read_int_callback,
	.set_termios =	     mct_u232_set_termios,
	.break_ctl =	     mct_u232_break_ctl,
	.tiocmget =	     mct_u232_tiocmget,
	.tiocmset =	     mct_u232_tiocmset,
	.attach =	     mct_u232_startup,
	.shutdown =	     mct_u232_shutdown,
};


struct mct_u232_private {
	spinlock_t lock;
	unsigned int	     control_state; /* Modem Line Setting (TIOCM) */
	unsigned char        last_lcr;      /* Line Control Register */
	unsigned char	     last_lsr;      /* Line Status Register */
	unsigned char	     last_msr;      /* Modem Status Register */
	unsigned int	     rx_flags;      /* Throttling flags */
};

#define THROTTLED		0x01


#define WDR_TIMEOUT 5000 /* default urb timeout */

static int mct_u232_calculate_baud_rate(struct usb_serial *serial,
					speed_t value, speed_t *result)
{
	*result = value;

	if (le16_to_cpu(serial->dev->descriptor.idProduct) == MCT_U232_SITECOM_PID
		|| le16_to_cpu(serial->dev->descriptor.idProduct) == MCT_U232_BELKIN_F5U109_PID) {
		switch (value) {
		case 300:
			return 0x01;
		case 600:
			return 0x02; /* this one not tested */
		case 1200:
			return 0x03;
		case 2400:
			return 0x04;
		case 4800:
			return 0x06;
		case 9600:
			return 0x08;
		case 19200:
			return 0x09;
		case 38400:
			return 0x0a;
		case 57600:
			return 0x0b;
		case 115200:
			return 0x0c;
		default:
			*result = 9600;
			return 0x08;
		}
	} else {
		/* FIXME: Can we use any divider - should we do
		   divider = 115200/value;
		   real baud = 115200/divider */
		switch (value) {
		case 300: break;
		case 600: break;
		case 1200: break;
		case 2400: break;
		case 4800: break;
		case 9600: break;
		case 19200: break;
		case 38400: break;
		case 57600: break;
		case 115200: break;
		default:
			value = 9600;
			*result = 9600;
		}
		return 115200/value;
	}
}

static int mct_u232_set_baud_rate(struct tty_struct *tty,
	struct usb_serial *serial, struct usb_serial_port *port, speed_t value)
{
	__le32 divisor;
	int rc;
	unsigned char zero_byte = 0;
	unsigned char cts_enable_byte = 0;
	speed_t speed;

	divisor = cpu_to_le32(mct_u232_calculate_baud_rate(serial, value,
								&speed));

	rc = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
				MCT_U232_SET_BAUD_RATE_REQUEST,
				MCT_U232_SET_REQUEST_TYPE,
				0, 0, &divisor, MCT_U232_SET_BAUD_RATE_SIZE,
				WDR_TIMEOUT);
	if (rc < 0)	/*FIXME: What value speed results */
		dev_err(&port->dev, "Set BAUD RATE %d failed (error = %d)\n",
			value, rc);
	else
		tty_encode_baud_rate(tty, speed, speed);
	dbg("set_baud_rate: value: 0x%x, divisor: 0x%x", value, divisor);

	/* Mimic the MCT-supplied Windows driver (version 1.21P.0104), which
	   always sends two extra USB 'device request' messages after the
	   'baud rate change' message.  The actual functionality of the
	   request codes in these messages is not fully understood but these
	   particular codes are never seen in any operation besides a baud
	   rate change.  Both of these messages send a single byte of data.
	   In the first message, the value of this byte is always zero.

	   The second message has been determined experimentally to control
	   whether data will be transmitted to a device which is not asserting
	   the 'CTS' signal.  If the second message's data byte is zero, data
	   will be transmitted even if 'CTS' is not asserted (i.e. no hardware
	   flow control).  if the second message's data byte is nonzero (a
	   value of 1 is used by this driver), data will not be transmitted to
	   a device which is not asserting 'CTS'.
	*/

	rc = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
				MCT_U232_SET_UNKNOWN1_REQUEST,
				MCT_U232_SET_REQUEST_TYPE,
				0, 0, &zero_byte, MCT_U232_SET_UNKNOWN1_SIZE,
				WDR_TIMEOUT);
	if (rc < 0)
		dev_err(&port->dev, "Sending USB device request code %d "
			"failed (error = %d)\n", MCT_U232_SET_UNKNOWN1_REQUEST,
			rc);

	if (port && C_CRTSCTS(tty))
	   cts_enable_byte = 1;

	dbg("set_baud_rate: send second control message, data = %02X",
							cts_enable_byte);
	rc = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			MCT_U232_SET_CTS_REQUEST,
			MCT_U232_SET_REQUEST_TYPE,
			0, 0, &cts_enable_byte, MCT_U232_SET_CTS_SIZE,
			WDR_TIMEOUT);
	if (rc < 0)
		dev_err(&port->dev, "Sending USB device request code %d "
			"failed (error = %d)\n", MCT_U232_SET_CTS_REQUEST, rc);

	return rc;
} /* mct_u232_set_baud_rate */

static int mct_u232_set_line_ctrl(struct usb_serial *serial, unsigned char lcr)
{
	int rc;
	rc = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			MCT_U232_SET_LINE_CTRL_REQUEST,
			MCT_U232_SET_REQUEST_TYPE,
			0, 0, &lcr, MCT_U232_SET_LINE_CTRL_SIZE,
			WDR_TIMEOUT);
	if (rc < 0)
		dev_err(&serial->dev->dev,
			"Set LINE CTRL 0x%x failed (error = %d)\n", lcr, rc);
	dbg("set_line_ctrl: 0x%x", lcr);
	return rc;
} /* mct_u232_set_line_ctrl */

static int mct_u232_set_modem_ctrl(struct usb_serial *serial,
				   unsigned int control_state)
{
	int rc;
	unsigned char mcr = MCT_U232_MCR_NONE;

	if (control_state & TIOCM_DTR)
		mcr |= MCT_U232_MCR_DTR;
	if (control_state & TIOCM_RTS)
		mcr |= MCT_U232_MCR_RTS;

	rc = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			MCT_U232_SET_MODEM_CTRL_REQUEST,
			MCT_U232_SET_REQUEST_TYPE,
			0, 0, &mcr, MCT_U232_SET_MODEM_CTRL_SIZE,
			WDR_TIMEOUT);
	if (rc < 0)
		dev_err(&serial->dev->dev,
			"Set MODEM CTRL 0x%x failed (error = %d)\n", mcr, rc);
	dbg("set_modem_ctrl: state=0x%x ==> mcr=0x%x", control_state, mcr);

	return rc;
} /* mct_u232_set_modem_ctrl */

static int mct_u232_get_modem_stat(struct usb_serial *serial,
						unsigned char *msr)
{
	int rc;
	rc = usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev, 0),
			MCT_U232_GET_MODEM_STAT_REQUEST,
			MCT_U232_GET_REQUEST_TYPE,
			0, 0, msr, MCT_U232_GET_MODEM_STAT_SIZE,
			WDR_TIMEOUT);
	if (rc < 0) {
		dev_err(&serial->dev->dev,
			"Get MODEM STATus failed (error = %d)\n", rc);
		*msr = 0;
	}
	dbg("get_modem_stat: 0x%x", *msr);
	return rc;
} /* mct_u232_get_modem_stat */

static void mct_u232_msr_to_state(unsigned int *control_state,
						unsigned char msr)
{
	/* Translate Control Line states */
	if (msr & MCT_U232_MSR_DSR)
		*control_state |=  TIOCM_DSR;
	else
		*control_state &= ~TIOCM_DSR;
	if (msr & MCT_U232_MSR_CTS)
		*control_state |=  TIOCM_CTS;
	else
		*control_state &= ~TIOCM_CTS;
	if (msr & MCT_U232_MSR_RI)
		*control_state |=  TIOCM_RI;
	else
		*control_state &= ~TIOCM_RI;
	if (msr & MCT_U232_MSR_CD)
		*control_state |=  TIOCM_CD;
	else
		*control_state &= ~TIOCM_CD;
	dbg("msr_to_state: msr=0x%x ==> state=0x%x", msr, *control_state);
} /* mct_u232_msr_to_state */


static int mct_u232_startup(struct usb_serial *serial)
{
	struct mct_u232_private *priv;
	struct usb_serial_port *port, *rport;

	priv = kzalloc(sizeof(struct mct_u232_private), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	spin_lock_init(&priv->lock);
	usb_set_serial_port_data(serial->port[0], priv);

	init_waitqueue_head(&serial->port[0]->write_wait);

	/* Puh, that's dirty */
	port = serial->port[0];
	rport = serial->port[1];
	/* No unlinking, it wasn't submitted yet. */
	usb_free_urb(port->read_urb);
	port->read_urb = rport->interrupt_in_urb;
	rport->interrupt_in_urb = NULL;
	port->read_urb->context = port;

	return 0;
} /* mct_u232_startup */


static void mct_u232_shutdown(struct usb_serial *serial)
{
	struct mct_u232_private *priv;
	int i;

	dbg("%s", __func__);

	for (i = 0; i < serial->num_ports; ++i) {
		/* My special items, the standard routines free my urbs */
		priv = usb_get_serial_port_data(serial->port[i]);
		if (priv) {
			usb_set_serial_port_data(serial->port[i], NULL);
			kfree(priv);
		}
	}
} /* mct_u232_shutdown */

static int  mct_u232_open(struct tty_struct *tty,
			struct usb_serial_port *port, struct file *filp)
{
	struct usb_serial *serial = port->serial;
	struct mct_u232_private *priv = usb_get_serial_port_data(port);
	int retval = 0;
	unsigned int control_state;
	unsigned long flags;
	unsigned char last_lcr;
	unsigned char last_msr;

	dbg("%s port %d", __func__, port->number);

	/* Compensate for a hardware bug: although the Sitecom U232-P25
	 * device reports a maximum output packet size of 32 bytes,
	 * it seems to be able to accept only 16 bytes (and that's what
	 * SniffUSB says too...)
	 */
	if (le16_to_cpu(serial->dev->descriptor.idProduct)
						== MCT_U232_SITECOM_PID)
		port->bulk_out_size = 16;

	/* Do a defined restart: the normal serial device seems to
	 * always turn on DTR and RTS here, so do the same. I'm not
	 * sure if this is really necessary. But it should not harm
	 * either.
	 */
	spin_lock_irqsave(&priv->lock, flags);
	if (tty && (tty->termios->c_cflag & CBAUD))
		priv->control_state = TIOCM_DTR | TIOCM_RTS;
	else
		priv->control_state = 0;

	priv->last_lcr = (MCT_U232_DATA_BITS_8 |
			  MCT_U232_PARITY_NONE |
			  MCT_U232_STOP_BITS_1);
	control_state = priv->control_state;
	last_lcr = priv->last_lcr;
	spin_unlock_irqrestore(&priv->lock, flags);
	mct_u232_set_modem_ctrl(serial, control_state);
	mct_u232_set_line_ctrl(serial, last_lcr);

	/* Read modem status and update control state */
	mct_u232_get_modem_stat(serial, &last_msr);
	spin_lock_irqsave(&priv->lock, flags);
	priv->last_msr = last_msr;
	mct_u232_msr_to_state(&priv->control_state, priv->last_msr);
	spin_unlock_irqrestore(&priv->lock, flags);

	port->read_urb->dev = port->serial->dev;
	retval = usb_submit_urb(port->read_urb, GFP_KERNEL);
	if (retval) {
		dev_err(&port->dev,
			"usb_submit_urb(read bulk) failed pipe 0x%x err %d\n",
			port->read_urb->pipe, retval);
		goto error;
	}

	port->interrupt_in_urb->dev = port->serial->dev;
	retval = usb_submit_urb(port->interrupt_in_urb, GFP_KERNEL);
	if (retval) {
		usb_kill_urb(port->read_urb);
		dev_err(&port->dev,
			"usb_submit_urb(read int) failed pipe 0x%x err %d",
			port->interrupt_in_urb->pipe, retval);
		goto error;
	}
	return 0;

error:
	return retval;
} /* mct_u232_open */


static void mct_u232_close(struct tty_struct *tty,
			struct usb_serial_port *port, struct file *filp)
{
	unsigned int c_cflag;
	unsigned int control_state;
	struct mct_u232_private *priv = usb_get_serial_port_data(port);
	dbg("%s port %d", __func__, port->number);

	if (tty) {
		c_cflag = tty->termios->c_cflag;
		mutex_lock(&port->serial->disc_mutex);
		if (c_cflag & HUPCL && !port->serial->disconnected) {
			/* drop DTR and RTS */
			spin_lock_irq(&priv->lock);
			priv->control_state &= ~(TIOCM_DTR | TIOCM_RTS);
			control_state = priv->control_state;
			spin_unlock_irq(&priv->lock);
			mct_u232_set_modem_ctrl(port->serial, control_state);
		}
		mutex_unlock(&port->serial->disc_mutex);
	}


	if (port->serial->dev) {
		/* shutdown our urbs */
		usb_kill_urb(port->write_urb);
		usb_kill_urb(port->read_urb);
		usb_kill_urb(port->interrupt_in_urb);
	}
} /* mct_u232_close */


static void mct_u232_read_int_callback(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	struct mct_u232_private *priv = usb_get_serial_port_data(port);
	struct usb_serial *serial = port->serial;
	struct tty_struct *tty;
	unsigned char *data = urb->transfer_buffer;
	int retval;
	int status = urb->status;
	unsigned long flags;

	switch (status) {
	case 0:
		/* success */
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* this urb is terminated, clean up */
		dbg("%s - urb shutting down with status: %d",
		    __func__, status);
		return;
	default:
		dbg("%s - nonzero urb status received: %d",
		    __func__, status);
		goto exit;
	}

	if (!serial) {
		dbg("%s - bad serial pointer, exiting", __func__);
		return;
	}

	dbg("%s - port %d", __func__, port->number);
	usb_serial_debug_data(debug, &port->dev, __func__,
					urb->actual_length, data);

	/*
	 * Work-a-round: handle the 'usual' bulk-in pipe here
	 */
	if (urb->transfer_buffer_length > 2) {
		tty = tty_port_tty_get(&port->port);
		if (urb->actual_length) {
			tty_insert_flip_string(tty, data, urb->actual_length);
			tty_flip_buffer_push(tty);
			tty_kref_put(tty);
		}
		goto exit;
	}

	/*
	 * The interrupt-in pipe signals exceptional conditions (modem line
	 * signal changes and errors). data[0] holds MSR, data[1] holds LSR.
	 */
	spin_lock_irqsave(&priv->lock, flags);
	priv->last_msr = data[MCT_U232_MSR_INDEX];

	/* Record Control Line states */
	mct_u232_msr_to_state(&priv->control_state, priv->last_msr);

#if 0
	/* Not yet handled. See belkin_sa.c for further information */
	/* Now to report any errors */
	priv->last_lsr = data[MCT_U232_LSR_INDEX];
	/*
	 * fill in the flip buffer here, but I do not know the relation
	 * to the current/next receive buffer or characters.  I need
	 * to look in to this before committing any code.
	 */
	if (priv->last_lsr & MCT_U232_LSR_ERR) {
		tty = tty_port_tty_get(&port->port);
		/* Overrun Error */
		if (priv->last_lsr & MCT_U232_LSR_OE) {
		}
		/* Parity Error */
		if (priv->last_lsr & MCT_U232_LSR_PE) {
		}
		/* Framing Error */
		if (priv->last_lsr & MCT_U232_LSR_FE) {
		}
		/* Break Indicator */
		if (priv->last_lsr & MCT_U232_LSR_BI) {
		}
		tty_kref_put(tty);
	}
#endif
	spin_unlock_irqrestore(&priv->lock, flags);
exit:
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval)
		dev_err(&port->dev,
			"%s - usb_submit_urb failed with result %d\n",
			__func__, retval);
} /* mct_u232_read_int_callback */

static void mct_u232_set_termios(struct tty_struct *tty,
				 struct usb_serial_port *port,
				 struct ktermios *old_termios)
{
	struct usb_serial *serial = port->serial;
	struct mct_u232_private *priv = usb_get_serial_port_data(port);
	struct ktermios *termios = tty->termios;
	unsigned int cflag = termios->c_cflag;
	unsigned int old_cflag = old_termios->c_cflag;
	unsigned long flags;
	unsigned int control_state;
	unsigned char last_lcr;

	/* get a local copy of the current port settings */
	spin_lock_irqsave(&priv->lock, flags);
	control_state = priv->control_state;
	spin_unlock_irqrestore(&priv->lock, flags);
	last_lcr = 0;

	/*
	 * Update baud rate.
	 * Do not attempt to cache old rates and skip settings,
	 * disconnects screw such tricks up completely.
	 * Premature optimization is the root of all evil.
	 */

	/* reassert DTR and RTS on transition from B0 */
	if ((old_cflag & CBAUD) == B0) {
		dbg("%s: baud was B0", __func__);
		control_state |= TIOCM_DTR | TIOCM_RTS;
		mct_u232_set_modem_ctrl(serial, control_state);
	}

	mct_u232_set_baud_rate(tty, serial, port, tty_get_baud_rate(tty));

	if ((cflag & CBAUD) == B0) {
		dbg("%s: baud is B0", __func__);
		/* Drop RTS and DTR */
		control_state &= ~(TIOCM_DTR | TIOCM_RTS);
		mct_u232_set_modem_ctrl(serial, control_state);
	}

	/*
	 * Update line control register (LCR)
	 */

	/* set the parity */
	if (cflag & PARENB)
		last_lcr |= (cflag & PARODD) ?
			MCT_U232_PARITY_ODD : MCT_U232_PARITY_EVEN;
	else
		last_lcr |= MCT_U232_PARITY_NONE;

	/* set the number of data bits */
	switch (cflag & CSIZE) {
	case CS5:
		last_lcr |= MCT_U232_DATA_BITS_5; break;
	case CS6:
		last_lcr |= MCT_U232_DATA_BITS_6; break;
	case CS7:
		last_lcr |= MCT_U232_DATA_BITS_7; break;
	case CS8:
		last_lcr |= MCT_U232_DATA_BITS_8; break;
	default:
		dev_err(&port->dev,
			"CSIZE was not CS5-CS8, using default of 8\n");
		last_lcr |= MCT_U232_DATA_BITS_8;
		break;
	}

	termios->c_cflag &= ~CMSPAR;

	/* set the number of stop bits */
	last_lcr |= (cflag & CSTOPB) ?
		MCT_U232_STOP_BITS_2 : MCT_U232_STOP_BITS_1;

	mct_u232_set_line_ctrl(serial, last_lcr);

	/* save off the modified port settings */
	spin_lock_irqsave(&priv->lock, flags);
	priv->control_state = control_state;
	priv->last_lcr = last_lcr;
	spin_unlock_irqrestore(&priv->lock, flags);
} /* mct_u232_set_termios */

static void mct_u232_break_ctl(struct tty_struct *tty, int break_state)
{
	struct usb_serial_port *port = tty->driver_data;
	struct usb_serial *serial = port->serial;
	struct mct_u232_private *priv = usb_get_serial_port_data(port);
	unsigned char lcr;
	unsigned long flags;

	dbg("%sstate=%d", __func__, break_state);

	spin_lock_irqsave(&priv->lock, flags);
	lcr = priv->last_lcr;

	if (break_state)
		lcr |= MCT_U232_SET_BREAK;
	spin_unlock_irqrestore(&priv->lock, flags);

	mct_u232_set_line_ctrl(serial, lcr);
} /* mct_u232_break_ctl */


static int mct_u232_tiocmget(struct tty_struct *tty, struct file *file)
{
	struct usb_serial_port *port = tty->driver_data;
	struct mct_u232_private *priv = usb_get_serial_port_data(port);
	unsigned int control_state;
	unsigned long flags;

	dbg("%s", __func__);

	spin_lock_irqsave(&priv->lock, flags);
	control_state = priv->control_state;
	spin_unlock_irqrestore(&priv->lock, flags);

	return control_state;
}

static int mct_u232_tiocmset(struct tty_struct *tty, struct file *file,
			      unsigned int set, unsigned int clear)
{
	struct usb_serial_port *port = tty->driver_data;
	struct usb_serial *serial = port->serial;
	struct mct_u232_private *priv = usb_get_serial_port_data(port);
	unsigned int control_state;
	unsigned long flags;

	dbg("%s", __func__);

	spin_lock_irqsave(&priv->lock, flags);
	control_state = priv->control_state;

	if (set & TIOCM_RTS)
		control_state |= TIOCM_RTS;
	if (set & TIOCM_DTR)
		control_state |= TIOCM_DTR;
	if (clear & TIOCM_RTS)
		control_state &= ~TIOCM_RTS;
	if (clear & TIOCM_DTR)
		control_state &= ~TIOCM_DTR;

	priv->control_state = control_state;
	spin_unlock_irqrestore(&priv->lock, flags);
	return mct_u232_set_modem_ctrl(serial, control_state);
}

static void mct_u232_throttle(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct mct_u232_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;
	unsigned int control_state;

	dbg("%s - port %d", __func__, port->number);

	spin_lock_irqsave(&priv->lock, flags);
	priv->rx_flags |= THROTTLED;
	if (C_CRTSCTS(tty)) {
		priv->control_state &= ~TIOCM_RTS;
		control_state = priv->control_state;
		spin_unlock_irqrestore(&priv->lock, flags);
		(void) mct_u232_set_modem_ctrl(port->serial, control_state);
	} else {
		spin_unlock_irqrestore(&priv->lock, flags);
	}
}


static void mct_u232_unthrottle(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct mct_u232_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;
	unsigned int control_state;

	dbg("%s - port %d", __func__, port->number);

	spin_lock_irqsave(&priv->lock, flags);
	if ((priv->rx_flags & THROTTLED) && C_CRTSCTS(tty)) {
		priv->rx_flags &= ~THROTTLED;
		priv->control_state |= TIOCM_RTS;
		control_state = priv->control_state;
		spin_unlock_irqrestore(&priv->lock, flags);
		(void) mct_u232_set_modem_ctrl(port->serial, control_state);
	} else {
		spin_unlock_irqrestore(&priv->lock, flags);
	}
}

static int __init mct_u232_init(void)
{
	int retval;
	retval = usb_serial_register(&mct_u232_device);
	if (retval)
		goto failed_usb_serial_register;
	retval = usb_register(&mct_u232_driver);
	if (retval)
		goto failed_usb_register;
	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
	       DRIVER_DESC "\n");
	return 0;
failed_usb_register:
	usb_serial_deregister(&mct_u232_device);
failed_usb_serial_register:
	return retval;
}


static void __exit mct_u232_exit(void)
{
	usb_deregister(&mct_u232_driver);
	usb_serial_deregister(&mct_u232_device);
}

module_init(mct_u232_init);
module_exit(mct_u232_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug enabled or not");
