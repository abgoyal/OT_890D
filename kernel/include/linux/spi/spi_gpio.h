
#ifndef __LINUX_SPI_GPIO_H
#define __LINUX_SPI_GPIO_H


struct spi_gpio_platform_data {
	unsigned	sck;
	unsigned	mosi;
	unsigned	miso;

	u16		num_chipselect;
};

#endif /* __LINUX_SPI_GPIO_H */
