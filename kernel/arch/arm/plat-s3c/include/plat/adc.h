

#ifndef __ASM_PLAT_ADC_H
#define __ASM_PLAT_ADC_H __FILE__

struct s3c_adc_client;

extern int s3c_adc_start(struct s3c_adc_client *client,
			 unsigned int channel, unsigned int nr_samples);

extern struct s3c_adc_client *s3c_adc_register(struct platform_device *pdev,
					       void (*select)(unsigned selected),
					       void (*conv)(unsigned d0, unsigned d1),
					       unsigned int is_ts);

extern void s3c_adc_release(struct s3c_adc_client *client);

#endif /* __ASM_PLAT_ADC_H */
