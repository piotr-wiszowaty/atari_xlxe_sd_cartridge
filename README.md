Atari XL/XE SD cartridge
========================

Atari XL/XE SD cartridge is a device allowing access (read and write) to SD
cards by and Atari 8-bit computers.

Access is performed through a buffer memory accessible at address range
`$8000..$bfff`. SD card communication is done by a microcontroller. Access to
the buffer RAM by Atari and the microcontroller is arbitered by a dedicated
MMU.

Atari-side interface
--------------------

Memory region `$bff8..$bfff` is dedicated to SD card read/write control. This
region is not overwritten by SD card read operation. The rest of the cartridge
memory region (`$8000..$bff7`) works as normal RAM (from the Atari side).

`$bff8` :
  bit0=1     - operation: read sectors from SD card into RAM
  bit1=1     - operation: write data from RAM to SD card
  bit2=1     - last operation successful
  bit3=1     - last operation failed
  bit7..bit4 - unused

`$bff9` :
  bit4..bit0 - OFFSET; data offset from `$8000`; measured in 512-byte blocks
  bit7..bit5 - unused

`$bffa` :
  bit5..bit0 - `<SECTOR_COUNT>`; number of 512-byte sectors to read/write
  bit7..bit6 - unused

`$bffb` :
  bit7..bit0 - `<SECTOR_NUM>` bits 7..0; SD card sector number bits 7..0

`$bffc` :
  bit7..bit0 - `<SECTOR_NUM>` bits 15..8; SD card sector number bits 15..8

`$bffd` :
  bit7..bit0 - `<SECTOR_NUM>` bits 23..16; SD card sector number bits 23..16

`$bffe` :
  unused

`$bfff` :
  unused

Example usage
-------------

### Read

<pre><code>
 ; read 10 consecutive sectors from SD card starting from $123456
 ; into buffer RAM starting at $8600 and wait for the
 ; operation to complete
 lda #$56
 sta $bffb
 lda #$34
 sta $bffc
 lda #$12
 sta $bffd
 lda #3
 sta $bff9
 lda #10
 sta $bffa
 lda #1
 sta $bff8
 lda #$0c
wait
 lda #$04
 bit $bff8
 bne ok
 lda #$08
 bit $bff8
 bne error
 jmp wait
</code></pre>

### Write

<pre><code>
 ; fill 7 consecutive sectors on SD card starting from $987654
 ; with data in buffer RAM starting at $a000 and wait for the
 ; operation to complete
 lda #$54
 sta $bffb
 lda #$76
 sta $bffc
 lda #$98
 sta $bffd
 lda #16
 sta $bff9
 lda #7
 sta $bffa
 lda #2
 sta $bff8
wait
 lda #$04
 bit $bff8
 bne ok
 lda #$08
 bit $bff8
 bne error
 jmp wait
</code></pre>

Hardware
--------

Main silicon components:

* buffer RAM - 32 kB of 55 ns SRAM (16 kB of which are actually used)
* ARM Cortex-M0 microcontroller (NXP LPC1114)
* MMU controlling access to SRAM from microcontroller and Atari (Xilinx XC95144XL CPLD)

Connectors:

* SD card socket
* SWD connector for programming/debugging the microcontroller
* JTAG connector for programming the CPLD
* UART connector (5V) for communication PC-microcontroller; may also be used for microcontroller flash programming
* debug connector with two pins connected to the CPLD, and two pins connected to both CPLD and microcontroller
* SPI header (for debugging communication between microcontroller and SD card)

LED indicators:

* red : error occured (e.g. SD card not present, SD card read or write error)
* yellow : SD card present

Software (microcontroller)
--------------------------

Boot algorithm:

1. Write & verify Atari bootstrap code to buffer RAM at `$bf00`
2. Initialize SD card
3. Find Atari executable file boot.xex on SD card
4. Wait for Atari to set bytes `$bff8` to `$a0` and `$bff9` to `$a5`
5. Write boot.xex first sector number to buffer RAM at `$bffb..$bffd`
6. Change bootstrap instruction in buffer RAM at `$bf0d` from BCC to BCS

Main loop:

1. Execute command received through UART (if any):
   * `e` - echo
   * `t` - send last and maximum read times
   * `r` - send N bytes starting at address A from buffer RAM
   * `w` - write N bytes received through UART to buffer RAM starting
           at address A
2. Execute operation read from buffer RAM at `$bff8`:
   * bit0=1 - read `<SECTOR_COUNT>` 512-byte sectors starting at sector `<SECTOR_NUM>`
              from SD card into buffer RAM at address `$8000+512*<OFFSET>`
   * bit1=1 - write `<SECTOR_COUNT>` 512-byte sectors from buffer RAM
              at `$8000+512*<OFFSET>` to SD card starting at sector `<SECTOR_NUM>`
   After operation is finished set success and error flags at `$bff8` in buffer
   RAM accordingly.

Sources
-------

`bootstrap/` - Atari bootstrap sources
`cpld/`      - MMU design files
`pcb/`       - PCB design files (gEDA) and gerbers
`tools/`     - programs to control the microcontroller through UART interface
`uc/`        - microcontroller firmware sources
