

#ifndef __CUST_GPIO_USAGE_H__
#define __CUST_GPIO_USAGE_H__


#define GPIO_CAMERA_CMRST_PIN         GPIO17
#define GPIO_CAMERA_CMRST_PIN_M_GPIO   GPIO_MODE_00

#define GPIO_CAMERA_CMPDN_PIN         GPIO18
#define GPIO_CAMERA_CMPDN_PIN_M_GPIO   GPIO_MODE_00

#define GPIO_HALL_2_PIN         GPIO21
#define GPIO_HALL_2_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_HALL_2_PIN_M_EINT   GPIO_MODE_01

#define GPIO_FM_RDS_PIN         GPIO22
#define GPIO_FM_RDS_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_FM_RDS_PIN_M_EINT   GPIO_MODE_01

#define GPIO_ALS_EINT_PIN         GPIO54
#define GPIO_ALS_EINT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_ALS_EINT_PIN_M_EINT   GPIO_MODE_02
#define GPIO_ALS_EINT_PIN_M_PWM   GPIO_MODE_01

#define GPIO_HALL_3_PIN         GPIO55
#define GPIO_HALL_3_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_HALL_3_PIN_M_EINT   GPIO_MODE_03
#define GPIO_HALL_3_PIN_M_PWM   GPIO_MODE_01

#define GPIO_OFN_EINT_PIN         GPIO55
#define GPIO_OFN_EINT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_OFN_EINT_PIN_M_EINT   GPIO_MODE_03
#define GPIO_OFN_EINT_PIN_M_PWM   GPIO_MODE_01

#define GPIO_HALL_4_PIN         GPIO57
#define GPIO_HALL_4_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_HALL_4_PIN_M_CLK   GPIO_MODE_01
#define GPIO_HALL_4_PIN_M_EINT   GPIO_MODE_02

#define GPIO_PMIC_EINT_PIN         GPIO59
#define GPIO_PMIC_EINT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_PMIC_EINT_PIN_M_EINT   GPIO_MODE_01

#define GPIO_BT_EINT_PIN         GPIO60
#define GPIO_BT_EINT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_BT_EINT_PIN_M_EINT   GPIO_MODE_01

#define GPIO_CAPTOUCH_EINT_PIN         GPIO61
#define GPIO_CAPTOUCH_EINT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_CAPTOUCH_EINT_PIN_M_EINT   GPIO_MODE_01

#define GPIO_MHALL_PIN         GPIO62
#define GPIO_MHALL_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_MHALL_PIN_M_EINT   GPIO_MODE_01

#define GPIO_PWR_BUTTON_PIN         GPIO63
#define GPIO_PWR_BUTTON_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_PWR_BUTTON_PIN_M_EINT   GPIO_MODE_01

#define GPIO_QWERTYSLIDE_EINT_PIN         GPIO63
#define GPIO_QWERTYSLIDE_EINT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_QWERTYSLIDE_EINT_PIN_M_EINT   GPIO_MODE_01

#define GPIO_WIFI_EINT_PIN         GPIO64
#define GPIO_WIFI_EINT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_WIFI_EINT_PIN_M_EINT   GPIO_MODE_01

#define GPIO_HALL_1_PIN         GPIO65
#define GPIO_HALL_1_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_HALL_1_PIN_M_EINT   GPIO_MODE_01

#define GPIO_HEADSET_INSERT_PIN         GPIO66
#define GPIO_HEADSET_INSERT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_HEADSET_INSERT_PIN_M_EINT   GPIO_MODE_01

#define GPIO_PCM_DAICLK_PIN         GPIO75
#define GPIO_PCM_DAICLK_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_PCM_DAICLK_PIN_M_CLK   GPIO_MODE_01

#define GPIO_PCM_DAIPCMOUT_PIN         GPIO76
#define GPIO_PCM_DAIPCMOUT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_PCM_DAIPCMOUT_PIN_M_DAIPCMOUT   GPIO_MODE_01

#define GPIO_PCM_DAIPCMIN_PIN         GPIO77
#define GPIO_PCM_DAIPCMIN_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_PCM_DAIPCMIN_PIN_M_DAIPCMIN   GPIO_MODE_01

#define GPIO_PCM_DAISYNC_PIN         GPIO79
#define GPIO_PCM_DAISYNC_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_PCM_DAISYNC_PIN_M_DAISYNC   GPIO_MODE_01

#define GPIO_WIFI_CLK_PIN         GPIO116
#define GPIO_WIFI_CLK_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_WIFI_CLK_PIN_M_CLK   GPIO_MODE_01
#define GPIO_WIFI_CLK_PIN_CLK     CLK_OUT1
#define GPIO_WIFI_CLK_PIN_FREQ    CLK_SRC_F32K

#define GPIO_BT_CLK_PIN         GPIO117
#define GPIO_BT_CLK_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_BT_CLK_PIN_M_CLK   GPIO_MODE_01
#define GPIO_BT_CLK_PIN_CLK     CLK_OUT2
#define GPIO_BT_CLK_PIN_FREQ    CLK_SRC_F32K

#define GPIO_FM_CLK_PIN         GPIO117
#define GPIO_FM_CLK_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_FM_CLK_PIN_M_CLK   GPIO_MODE_01
#define GPIO_FM_CLK_PIN_CLK     CLK_OUT2
#define GPIO_FM_CLK_PIN_FREQ    CLK_SRC_F32K

#define GPIO_GPS_CLK_PIN         GPIO118
#define GPIO_GPS_CLK_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_GPS_CLK_PIN_M_CLK   GPIO_MODE_01
#define GPIO_GPS_CLK_PIN_CLK     CLK_OUT3
#define GPIO_GPS_CLK_PIN_FREQ    CLK_SRC_F32K

#define GPIO_OFN_RST_PIN         GPIO119
#define GPIO_OFN_RST_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_OFN_RST_PIN_M_CLK   GPIO_MODE_01
#define GPIO_OFN_RST_PIN_CLK     CLK_OUT4
#define GPIO_OFN_RST_PIN_FREQ    GPIO_CLKSRC_NONE

#define GPIO_BT_RESET_PIN         GPIO122
#define GPIO_BT_RESET_PIN_M_GPIO   GPIO_MODE_00

#define GPIO_GPS_RST_PIN         GPIO123
#define GPIO_GPS_RST_PIN_M_GPIO   GPIO_MODE_00

#define GPIO_OFN_DWN_PIN         GPIO124
#define GPIO_OFN_DWN_PIN_M_GPIO   GPIO_MODE_00

#define GPIO_BT_POWREN_PIN         GPIO132
#define GPIO_BT_POWREN_PIN_M_GPIO   GPIO_MODE_00

#define GPIO_WIFI_RST_PIN         GPIO133
#define GPIO_WIFI_RST_PIN_M_GPIO   GPIO_MODE_00


/*Output for default variable names*/
/*@XXX_XX_PIN in gpio.cmp          */



#endif /* __CUST_GPIO_USAGE_H__ */


