
#ifndef __VIAUTILITY_H__
#define __VIAUTILITY_H__

/* These functions are used to get infomation about device's state */
void viafb_get_device_support_state(u32 *support_state);
void viafb_get_device_connect_state(u32 *connect_state);
bool viafb_lcd_get_support_expand_state(u32 xres, u32 yres);

/* These function are used to access gamma table */
void viafb_set_gamma_table(int bpp, unsigned int *gamma_table);
void viafb_get_gamma_table(unsigned int *gamma_table);
void viafb_get_gamma_support_state(int bpp, unsigned int *support_state);
int viafb_input_parameter_converter(int parameter_value);

#endif /* __VIAUTILITY_H__ */
