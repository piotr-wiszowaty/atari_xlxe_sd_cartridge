/**
 *  Atari XL/XE SD cartridge
 *  Copyright (C) 2013  Piotr Wiszowaty
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see {http://www.gnu.org/licenses/}.
 */

#include <stdint.h>
#include "sdmmc.h"

#define CMD41_COUNT		10000
#define TOKEN_COUNT		100000

volatile int block_addressing;

int sdmmc_cmd(uint8_t cmd_code, uint32_t arg)
{
	int i;
	uint8_t result;

	cs_low();
	while (spi(0xff) != 0xff);
	spi(0x40 | cmd_code);
	spi((arg >> 24) & 0xff);
	spi((arg >> 16) & 0xff);
	spi((arg >> 8) & 0xff);
	spi(arg & 0xff);
	spi(cmd_code == 8 ? 0x87 : 0x95);
	if (cmd_code == 12) {
		spi(0xff);		// discard stuff byte
	}
	for (i = 0; i < 256; i++) {
		result = spi(0xff);
		if ((result & 0x80) == 0) {
			return result;
		}
	}

	return -1;
}

uint32_t sdmmc_read_word()
{
	return ((uint32_t) spi(0xff) << 24) | ((uint32_t) spi(0xff) << 16) | ((uint32_t) spi(0xff) << 8) | ((uint32_t) spi(0xff));
}

void sdmmc_cmd_term()
{
	cs_high();
	spi(0xff);
}

int sdmmc_init()
{
	int i;
	int result;

	// send > 74 clock pulses with CS = DI = 1
	cs_high();
	for (i = 0; i < 10; i++) {
		spi(0xff);
	}

	// CMD0 - GO_IDLE_STATE
	result = sdmmc_cmd(0, 0);
	sdmmc_cmd_term();
	if (result != 0x01) {
		return -1;
	}

	// CMD8 - SEND_IF_COND
	result = sdmmc_cmd(8, 0x1aa);
	if (result != 0x01) {
		// TODO: initialize SD v1 or MMC v3
		sdmmc_cmd_term();
		return -2;
	}
	if ((result = sdmmc_read_word()) != 0x1aa) {
		sdmmc_cmd_term();
		return -3;
	}
	sdmmc_cmd_term();

	for (i = 0; i < CMD41_COUNT; i++) {
		// CMD55 - APP_CMD
		result = sdmmc_cmd(55, 0);
		sdmmc_cmd_term();
		if (result < 0) {
			return -5;
		}

		// CMD41 - APP_SEND_OP_COND
		result = sdmmc_cmd(41, 0x40000000);
		sdmmc_cmd_term();
		if (result == 0x00) {
			break;
		} else if (result != 0x01) {
			return -6;
		}
	}
	if (i == CMD41_COUNT) {
		return -7;
	}

	// CMD58 - READ_OCR
	result = sdmmc_cmd(58, 0);
	if (result < 0) {
		sdmmc_cmd_term();
		return -8;
	}
	if ((result = sdmmc_read_word()) & 0x40000000) {
		block_addressing = 1;
	} else {
		block_addressing = 0;
		sdmmc_cmd_term();
		// CMD16 - SET_BLOCK_LEN (512 bytes)
		result = sdmmc_cmd(16, 512);
		if (result < 0) {
			sdmmc_cmd_term();
			return -9;
		}
	}
	sdmmc_cmd_term();

	return 0;
}

int sdmmc_read_block(uint32_t address, uint8_t *buffer)
{
	int i;
	int result;

	// CMD17 - READ_SINGLE_BLOCK
	result = sdmmc_cmd(17, block_addressing ? address : address << 9);
	if (result < 0) {
		cs_high();
		return -1;
	}

	// wait for data token
	for (i = 0; i < TOKEN_COUNT; i++) {
		if (spi(0xff) == 0xfe) {
			break;
		}
	}
	if (i == TOKEN_COUNT) {
		return -2;
	}

	// read data
	for (i = 0; i < 512; i++) {
		*buffer++ = spi(0xff);
	}
	
	// skip CRC
	spi(0xff);
	spi(0xff);

	sdmmc_cmd_term();

	return 0;
}

int sdmmc_read_multiple_blocks_start(uint32_t address)
{
	int result;

	if ((result = sdmmc_cmd(18, block_addressing ? address : address << 9)) < 0) {
		cs_high();
		return result;
	}

	return 0;
}

int sdmmc_stop_transmission()
{
	int result;

	if ((result = sdmmc_cmd(12, 0)) < 0) {
		while (spi(0xff) != 0xff);
		cs_high();
		return result;
	}
	return 0;
}

int sdmmc_write_multiple_blocks(uint32_t address, uint32_t n_blocks, uint8_t (*next_byte)())
{
	int result;
	int i;

	if ((result = sdmmc_cmd(25, block_addressing ? address : address << 9)) < 0) {
		cs_high();
		return result;
	}

	spi(0xff);							// initiate write

	while (n_blocks--) {
		spi(0xfc);						// data token
		for (i = 0; i < 512; i++) {		// sector data
			spi(next_byte());
		}
		spi(0xff);						// CRC
		spi(0xff);
		result = spi(0xff);				// data response
		if ((result & 0x1f) != 0x05) {
			cs_high();
			return result;
		}
		i = 0;
		do {							// wait while card is busy
			if (++i == 131072) {
				cs_high();
				return -10;
			}
		} while (spi(0xff) != 0xff);
	}

	return sdmmc_stop_transmission();
}
