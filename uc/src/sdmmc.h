#ifndef SDMMC_H_
#define SDMMC_H_

extern unsigned char spi(unsigned char data);
extern void cs_low();
extern void cs_high();

int sdmmc_init();
int sdmmc_read_block(uint32_t address, uint8_t *buffer);
int sdmmc_write_block(uint32_t address, uint8_t *buffer);
int sdmmc_read_multiple_blocks_start(uint32_t address);
int sdmmc_stop_transmission();
int sdmmc_write_multiple_blocks(uint32_t address, uint32_t n_blocks, uint8_t (*next_byte)());

#endif
