

#ifndef _DAVINCI_PCM_H
#define _DAVINCI_PCM_H

struct davinci_pcm_dma_params {
	char *name;		/* stream identifier */
	int channel;		/* sync dma channel ID */
	dma_addr_t dma_addr;	/* device physical address for DMA */
	unsigned int data_type;	/* xfer data type */
};

struct evm_snd_platform_data {
	int tx_dma_ch;
	int rx_dma_ch;
};

extern struct snd_soc_platform davinci_soc_platform;

#endif
