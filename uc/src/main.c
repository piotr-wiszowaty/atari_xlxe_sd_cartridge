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

#include "LPC11xx.h"
#include "sdmmc.h"
#include "fat.h"
#include "bootstrap.h"

#define OFFSET_D5E8		0x35e8
#define OFFSET_8000		0x4000
#define OFFSET_A000		0x6000
#define OFFSET_BOOTSTRAP (OFFSET_A000 + 8192 - 256)
#define CART_SIZE		16384
#define TOTAL_CART_SECTORS 32

#define EN_TIMER16_0	(1 << 7)
#define EN_TIMER16_1	(1 << 8)
#define EN_TIMER32_0	(1 << 9)
#define EN_TIMER32_1	(1 << 10)
#define EN_SSP			(1 << 11)
#define EN_UART			(1 << 12)
#define EN_USBREG		(1 << 14)
#define EN_IOCON		(1 << 16)

// GPIO0
#define AUX1			(1 << 2)
#define AUX3			(1 << 3)
#define SET_ADDR_LO		(1 << 4)
#define SET_ADDR_HI		(1 << 5)
#define STROBE_ADDR		(1 << 6)
#define READ			(1 << 7)
#define MISO			(1 << 8)
#define MOSI			(1 << 9)
#define WPROTECT		(1 << 11)

// GPIO1
#define LED2			(1 << 5)
#define RXD				(1 << 6)
#define TXD				(1 << 7)
#define AUX0			(1 << 8)
#define WRITE			(1 << 9)
#define CS				(1 << 10)

// GPIO2
#define DATA			(0xff << 0)
#define AUX4			(1 << 8)
#define AUX5			(1 << 9)
#define CDETECT			(1 << 10)
#define SCK				(1 << 11)

// GPIO3
#define ACK				(1 << 2)
#define LED1			(1 << 3)

#define SPI0_SLOW		100
#define SPI0_FAST		2

#define CARD_PRESENT	(LPC_GPIO2->MASKED_ACCESS[CDETECT] == 0)
#define CARD_WRPROTECT	LPC_GPIO0->MASKED_ACCESS[WRPROTECT]

#define YELLOW_LED_ON	LPC_GPIO1->MASKED_ACCESS[LED2] = 0
#define YELLOW_LED_OFF	LPC_GPIO1->MASKED_ACCESS[LED2] = LED2
#define RED_LED_ON		LPC_GPIO3->MASKED_ACCESS[LED1] = 0
#define RED_LED_OFF		LPC_GPIO3->MASKED_ACCESS[LED1] = LED1

#define PANIC			RED_LED_ON; \
						while (1)

#define DATA_IN			LPC_GPIO2->DIR &= ~DATA
#define DATA_OUT		LPC_GPIO2->DIR |= DATA

#define CMD_MASK_READ	    0x01
#define CMD_MASK_WRITE	    0x02
#define CMD_MASK_SUCCESS	0x40
#define CMD_MASK_FAILURE	0x80

#define FETCH_CMD_PARAMS	\
	sector_offset = ram_read() & 0x1f;	\
	n_sectors = ram_read() & 0x1f;		\
	sector = ram_read();				\
	sector |= (int) ram_read() << 8;	\
	sector |= (int) ram_read() << 16

extern void transfer_sector_to_ram();

void blinking_panic()
{
	int i;

	while (1) {
		RED_LED_ON;
		for (i = 0; i < 1000000; i++) {
			asm volatile ("\n\tnop");
		}
		RED_LED_OFF;
		for (i = 0; i < 1000000; i++) {
			asm volatile ("\n\tnop");
		}
	}
}

void cs_low()
{
	LPC_GPIO1->MASKED_ACCESS[CS] = 0;
}

void cs_high()
{
	LPC_GPIO1->MASKED_ACCESS[CS] = CS;
}

unsigned char spi(unsigned char data)
{
	LPC_SSP0->DR = data;
	while (LPC_SSP0->SR & 0x10);
	return LPC_SSP0->DR;
}

int rx()
{
	if (LPC_UART->LSR & 0x01) {
		return LPC_UART->RBR & 0xff;
	} else {
		return -1;
	}
}

int rx_byte()
{
	while (!(LPC_UART->LSR & 0x01));
	return LPC_UART->RBR & 0xff;
}

int rx_word()
{
	return rx_byte() | (rx_byte() << 8);
}

void tx(char c)
{
	while (!(LPC_UART->LSR & 0x40));
	LPC_UART->THR = c;
}

void tx_int(int i)
{
	tx(i & 0xff);
	tx((i >> 8) & 0xff);
	tx((i >> 16) & 0xff);
	tx((i >> 24) & 0xff);
}

void ram_set_address(int addr)
{
	DATA_OUT;
	LPC_GPIO2->MASKED_ACCESS[DATA] = addr;
	LPC_GPIO0->MASKED_ACCESS[SET_ADDR_LO] = -1;
	LPC_GPIO0->MASKED_ACCESS[STROBE_ADDR] = -1;
	LPC_GPIO0->MASKED_ACCESS[STROBE_ADDR | SET_ADDR_LO] = 0;
	LPC_GPIO2->MASKED_ACCESS[DATA] = addr >> 8;
	LPC_GPIO0->MASKED_ACCESS[SET_ADDR_HI] = -1;
	LPC_GPIO0->MASKED_ACCESS[STROBE_ADDR] = -1;
	LPC_GPIO0->MASKED_ACCESS[SET_ADDR_HI | STROBE_ADDR] = 0;
}

void ram_write(char data)
{
	LPC_GPIO2->MASKED_ACCESS[DATA] = data;

	asm volatile (
		"  str %[ones], [%[wr_a]]    \n\t"
		"write_loop:                 \n\t"
		"  ldr r0, [%[ack_a]]        \n\t"
		"  tst r0, %[ones]           \n\t"
		"  beq write_loop            \n\t"
		"  str %[zero], [%[wr_a]]    \n\t"
		"  str %[ones], [%[strob_a]] \n\t"
		"  str %[zero], [%[strob_a]] \n\t"
		: // output
		: [zero] "r" (0),
		  [ones] "r" (-1),
		  [wr_a] "r" (&LPC_GPIO1->MASKED_ACCESS[WRITE]),
		  [ack_a] "r" (&LPC_GPIO3->MASKED_ACCESS[ACK]),
		  [strob_a] "r" (&LPC_GPIO0->MASKED_ACCESS[STROBE_ADDR])
		: "cc", "r0");
}

unsigned char ram_read()
{
	unsigned char c;
	LPC_GPIO0->MASKED_ACCESS[READ] = -1;
	while (!LPC_GPIO3->MASKED_ACCESS[ACK]);
	c = LPC_GPIO2->MASKED_ACCESS[DATA];
	LPC_GPIO0->MASKED_ACCESS[READ] = 0;
	LPC_GPIO0->MASKED_ACCESS[STROBE_ADDR] = -1;
	LPC_GPIO0->MASKED_ACCESS[STROBE_ADDR] = 0;
	return c;
}

void clean_d5()
{
	int i;

	ram_set_address(OFFSET_D5E8);
	for (i = 0; i < 8; i++) {
		ram_write(0);
	}
}

int main()
{
	int i;
	int n;
	int c;
	int sector = 0;
	int command;
	int n_sectors;
	int sector_offset;
	int uart_div;
	int t_1st;
	int t_2nd;
	int t_total;
	int max_t_1st = 0;
	int max_t_2nd = 0;
	int max_t_total = 0;
	int active_read_sector = 0;
	int result;

	LPC_SYSCON->SYSAHBCLKCTRL |= EN_IOCON | EN_SSP | EN_UART | EN_TIMER32_0;

	LPC_GPIO0->DIR |= AUX3 | SET_ADDR_LO | SET_ADDR_HI | STROBE_ADDR | READ;
	LPC_GPIO0->MASKED_ACCESS[AUX3 | SET_ADDR_LO | SET_ADDR_HI | STROBE_ADDR | READ] = 0;

	LPC_GPIO1->DIR |= LED2 | CS | WRITE;
	LPC_GPIO1->MASKED_ACCESS[LED2 | CS | WRITE] = LED2 | CS;

	LPC_GPIO2->DIR |= AUX4 | AUX5;

	LPC_GPIO3->DIR |= LED1;
	LPC_GPIO3->MASKED_ACCESS[LED1] = LED1;

	LPC_IOCON->R_PIO0_11 = 0x91;			// digital mode, pull-up

	LPC_SYSCON->PRESETCTRL |= 1;			// un-reset SSP0
	LPC_IOCON->PIO0_8 = 0x01;				// MISO0
	LPC_IOCON->PIO0_9 = 0x01;				// MOSI0
	LPC_IOCON->SCK_LOC = 1;					// SCK on PIO2_11
	LPC_IOCON->PIO2_11 = 0x01;				// SCK
	LPC_SYSCON->SSP0CLKDIV = SPI0_SLOW;		// SSP0 clock divider
	LPC_SSP0->CPSR = 2;
	LPC_SSP0->CR0 = 0x0007;					// 8-bit transfers
	LPC_SSP0->CR1 = 0x0002;					// enable SSP0, master mode

	LPC_IOCON->PIO1_6 = 0x11;				// RXD, pull-up
	LPC_IOCON->PIO1_7 = 0x01;				// TXD
	LPC_SYSCON->UARTCLKDIV = 1;
	uart_div = SystemCoreClock / 16 / 19200;
	LPC_UART->LCR = 0x80;
	LPC_UART->DLL = uart_div & 0xff;
	LPC_UART->DLM = (uart_div >> 8) & 0xff;
	LPC_UART->FCR = 0x07;					// enable & reset FIFOs
	LPC_UART->LCR = 0x03;					// 8-bit data, 1 stop bit

	LPC_TMR32B0->PR = SystemCoreClock / 1000000 - 1;	// 1 us resolution
	LPC_TMR32B0->TCR = 2;								// reset

	// wait 100 ms
	LPC_TMR32B0->MR0 = 100 * 1000;
	LPC_TMR32B0->MCR = 0x06;				// stop & reset on MR0 match
	LPC_TMR32B0->TCR = 1;					// start
	while (LPC_TMR32B0->TCR & 1);

	// output system clock on CLKO pin
	LPC_SYSCON->CLKOUTDIV = 1;
	LPC_SYSCON->CLKOUTCLKSEL = 3;			// main clock
	LPC_SYSCON->CLKOUTUEN = 0;
	LPC_SYSCON->CLKOUTUEN = 1;
	LPC_IOCON->PIO0_1 = 0xc1;				// CLKO

	clean_d5();

	// write bootstrap code to RAM
	ram_set_address(OFFSET_BOOTSTRAP);
	for (i = 0; i < 256; i++) {
		ram_write(bootstrap[i]);
	}
	// verify written bootstrap
	ram_set_address(OFFSET_BOOTSTRAP);
	DATA_IN;
	for (i = 0; i < 256; i++) {
		if ((c = ram_read()) != bootstrap[i]) {
			blinking_panic();
		}
	}

	// initialize SD card
	if (!CARD_PRESENT) {
		PANIC;
	}
	if ((i = sdmmc_init()) < 0) {
		PANIC;
	} else {
		LPC_SYSCON->SSP0CLKDIV = SPI0_FAST;
	}

	// indicate successful SD card initialization
	YELLOW_LED_ON;

	// find executable file on SD card
	if ((sector = fat_find_first_sector("boot.xex")) == 0) {
		PANIC;
	}

	// initialize sector-number-to-load in RAM
	ram_set_address(OFFSET_D5E8 + 3);
	ram_write((sector >>  0) & 0xff);
	ram_write((sector >>  8) & 0xff);
	ram_write((sector >> 16) & 0xff);

	while (1) {
		if ((c = rx()) > -1) {
			switch (c) {
				case 'e':
					tx('e');
					break;
				case 't':
					tx('t');
					tx_int(t_1st);
					tx_int(max_t_1st);
					tx_int(t_2nd);
					tx_int(max_t_2nd);
					tx_int(t_total);
					tx_int(max_t_total);
					break;
				case 'r':
					ram_set_address(rx_word());
					n = rx_word();
					DATA_IN;
					for (i = 0; i < n; i++) {
						tx(ram_read());
					}
					break;
				case 'w':
					ram_set_address(rx_word());
					n = rx_word();
					for (i = 0; i < n; i++) {
						ram_write(rx_byte());
					}
					tx('w');
					break;
				default:
					break;
			}
		}

		ram_set_address(OFFSET_D5E8);
		DATA_IN;
		command = ram_read();
		if (command & CMD_MASK_READ) {		 // read sector(s)?
			FETCH_CMD_PARAMS;
			if (n_sectors) {
				LPC_TMR32B0->TCR = 1;

				if (active_read_sector != 0 && active_read_sector != sector) {
					sdmmc_stop_transmission();
				}
				result = 0;
				if (active_read_sector == 0 || active_read_sector != sector) {
					result = sdmmc_read_multiple_blocks_start(sector);
				}
				if (result) {
					ram_set_address(OFFSET_D5E8);
					ram_write(CMD_MASK_FAILURE);
					active_read_sector = 0;
					sdmmc_stop_transmission();
					continue;
				} else {
					active_read_sector = sector + n_sectors;
				}

				ram_set_address(OFFSET_8000 + (sector_offset << 9));

				for (i = 0; i < n_sectors; i++) {
					if (sector_offset + i >= TOTAL_CART_SECTORS) {
						ram_set_address(OFFSET_8000);
					}
					transfer_sector_to_ram();
					if (i == 0) {
						t_1st = LPC_TMR32B0->TC;
					} else if (i == 1) {
						t_2nd = LPC_TMR32B0->TC;
					}
				}
				t_total = LPC_TMR32B0->TC;

				ram_set_address(OFFSET_D5E8);
				ram_write(CMD_MASK_SUCCESS);

				LPC_TMR32B0->TCR = 2;

				if (t_total > 20000) {
					RED_LED_ON;
				} else {
					RED_LED_OFF;
				}

				if (t_1st > max_t_1st) {
					max_t_1st = t_1st;
				}
				if (t_2nd > max_t_2nd) {
					max_t_2nd = t_2nd;
				}
				if (t_total > max_t_total) {
					max_t_total = t_total;
				}
			}
		} else if (command & CMD_MASK_WRITE) {	// write sector(s)?
			FETCH_CMD_PARAMS;
			if (n_sectors) {
				if (active_read_sector) {
					active_read_sector = 0;
					sdmmc_stop_transmission();
				}

				ram_set_address(OFFSET_8000 + (sector_offset << 9));
				DATA_IN;
				if (sector_offset + n_sectors > TOTAL_CART_SECTORS) {
					// write 2 chunks of sectors
					result = sdmmc_write_multiple_blocks(sector, TOTAL_CART_SECTORS - sector_offset, ram_read);
					if (!result) {
						ram_set_address(OFFSET_8000);
						DATA_IN;
						result = sdmmc_write_multiple_blocks(sector, n_sectors - (TOTAL_CART_SECTORS - sector_offset), ram_read);
					}
				} else {
					// write 1 chunk of sectors
					result = sdmmc_write_multiple_blocks(sector, n_sectors, ram_read);
				}
				ram_set_address(OFFSET_D5E8);
				if (result < 0) {
					ram_write(CMD_MASK_FAILURE);
				} else {
					ram_write(CMD_MASK_SUCCESS);
				}
			}
		}
	}
}
