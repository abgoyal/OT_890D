

#ifndef STV06XX_SENSOR_H_
#define STV06XX_SENSOR_H_

#include "stv06xx.h"

#define IS_850(sd)	((sd)->gspca_dev.dev->descriptor.idProduct == 0x850)
#define IS_870(sd)	((sd)->gspca_dev.dev->descriptor.idProduct == 0x870)
#define IS_1020(sd)	((sd)->sensor == &stv06xx_sensor_hdcs1020)

extern const struct stv06xx_sensor stv06xx_sensor_vv6410;
extern const struct stv06xx_sensor stv06xx_sensor_hdcs1x00;
extern const struct stv06xx_sensor stv06xx_sensor_hdcs1020;
extern const struct stv06xx_sensor stv06xx_sensor_pb0100;

#define STV06XX_MAX_CTRLS		(V4L2_CID_LASTP1 - V4L2_CID_BASE + 10)

struct stv06xx_sensor {
	/* Defines the name of a sensor */
	char name[32];

	/* Sensor i2c address */
	u8 i2c_addr;

	/* Flush value*/
	u8 i2c_flush;

	/* length of an i2c word */
	u8 i2c_len;

	/* Probes if the sensor is connected */
	int (*probe)(struct sd *sd);

	/* Performs a initialization sequence */
	int (*init)(struct sd *sd);

	/* Executed at device disconnect */
	void (*disconnect)(struct sd *sd);

	/* Reads a sensor register */
	int (*read_sensor)(struct sd *sd, const u8 address,
	      u8 *i2c_data, const u8 len);

	/* Writes to a sensor register */
	int (*write_sensor)(struct sd *sd, const u8 address,
	      u8 *i2c_data, const u8 len);

	/* Instructs the sensor to start streaming */
	int (*start)(struct sd *sd);

	/* Instructs the sensor to stop streaming */
	int (*stop)(struct sd *sd);

	/* Instructs the sensor to dump all its contents */
	int (*dump)(struct sd *sd);

	int nctrls;
	struct ctrl ctrls[STV06XX_MAX_CTRLS];

	char nmodes;
	struct v4l2_pix_format modes[];
};

#endif
