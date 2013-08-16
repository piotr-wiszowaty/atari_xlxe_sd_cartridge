//    Atari XL/XE SD cartridge
//    Copyright (C) 2013  Piotr Wiszowaty
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see http://www.gnu.org/licenses/.

	// GPIO0
	.equ	STROBE_ADDR, (1 << 6)

        // GPIO1
	.equ	WRITE, (1 << 9)

	// GPIO3
	.equ	ACK, (1 << 2)

.macro	waitrne
1:	ldr	r0, [r3]
	tst	r0, r5
	beq	1b
.endm

.macro	spirx
	ldr	r0, [r4]
.endm

.macro	spirxtx
	ldr	r0, [r4]
	str	r6, [r4]
.endm

.macro	add1hi	reg
	mov	r0, \reg
	add	r0, #1
	mov	\reg, r0
.endm

.macro	str_l	where
	str	r7, \where
.endm

.macro	str_h	where
	str	r6, \where
.endm

	.cpu	cortex-m0
	.text
	.thumb
	.align	2
	.global	transfer_sector_to_ram
	.thumb_func
transfer_sector_to_ram:
	push	{r0-r7}
	mov	r7, r8
	mov	r6, r9
	mov	r5, r10
	mov	r4, r11
	push	{r4-r7}

	mov	r10, r0			// n_bytes

	ldr	r3, ssp0_sr
	ldr	r4, ssp0_dr
	ldr	r5, rne_mask
	ldr	r6, minus1
	ldr	r7, zero

	ldr	r0, zero
	mov	r8, r0			// byte counter
	ldr	r0, sector_size
	mov	r9, r0
	mov	r0, #0xfe
	mov	r11, r0

// ldr r1,=carcass
// ldr r0,[r1]
// ldr r1,=tmp
// str r0,[r1]

	str_h	[r4]
data_token_loop:
	waitrne
	spirxtx
	cmp	r0, r11
	bne	data_token_loop

transfer_loop:
	waitrne
	spirxtx
	cmp	r8, r10
	bge	next_byte
// ldr r1,=tmp
// ldr r0,[r1]
// mov r2,#8
// ror r0,r2
// str r0,[r1]
	ldr	r1, io_data
	str	r0, [r1]
	ldr	r1, write
	str_h	[r1]
	ldr	r2, ack
write_loop:
	ldr	r0, [r2]
	tst	r0, r0
	beq	write_loop
	str_l	[r1]
	ldr	r0, strobe_a
	str_h	[r0]
	str_l	[r0]
next_byte:
	add1hi	r8
	cmp	r8, r9
	bne	transfer_loop

	// skip CRC
	waitrne
	spirxtx
	waitrne
	spirx

	pop	{r4-r7}
	mov	r8, r7
	mov	r9, r6
	mov	r10, r5
	mov	r11, r4
	pop	{r0-r7}
	bx	lr

	.align	2

zero:     .word 0
one:      .word 1
minus1:   .word 0xffffffff
rne_mask: .word 0x04
ssp0_sr:  .word 0x4004000C
ssp0_dr:  .word 0x40040008
io_data:  .word 0x50000000 + 0x20000 + (0xff << 2)
write:    .word 0x50000000 + 0x10000 + (WRITE << 2)
ack:      .word 0x50000000 + 0x30000 + (ACK << 2)
strobe_a: .word 0x50000000 + 0x00000 + (STROBE_ADDR << 2)
//carcass:  .word 0xbeaddeef

max_sectors: .word 16
sector_size: .word 512

//	.bss
//tmp:	.space	4

	.end
