Atari XL/XE SD cartridge
========================

Atari XL/XE SD cartridge is a device allowing usage of SD cards
by and Atari 8-bit computers.

Hardware

Main silicon components are:
* 32 kB of 55 ns SRAM (16 kB of which are actually used)
* ARM Cortex-M0 microcontroller (NXP LPC1114)
* MMU controlling access to SRAM from microcontroller and Atari
  (Xilinx XC95144XL CPLD)

Connectors:
* SD card socket
* SWD connector for programming/debugging the microcontroller
* JTAG connector for programming the CPLD
* UART connector (5V) for communication PC-microcontroller; may also be used
  for microcontroller flash programming
* debug connector with two pins connected to the CPLD, and two pins
  connected to both CPLD and microcontroller

LED indicators:
* red : error occured (e.g. SD card not present, SD card read or write error)
* yellow : SD card present

Software (microcontroller)

Boot algorithm:
1. Write & verify Atari bootstrap code to external RAM at $BF00
2. Initialize SD card
3. Find Atari executable file boot.xex on SD card
4. Wait for Atari to set bytes $BFF8 to $A0 and $BFF9 to $A5
5. Write boot.xex first sector number to external RAM at $BFFB..$BFFD
6. Change bootstrap instruction in external RAM at $BF0D from BCC to BCS

Main loop:
1. Execute command received through UART (if any):
   * 'e' - echo
   * 't' - send last and maximum read times
   * 'r' - send N bytes starting at address A from external RAM
   * 'w' - write N bytes received through UART to external RAM starting
           at address A
2. Execute operation read from external RAM at $BFF8:
   * bit0=1 - read <SECTOR_COUNT> 512-byte sectors starting at sector <SECTOR_NUM>
              from SD card into external RAM at address $8000+512*<OFFSET>
   * bit1=1 - write <SECTOR_COUNT> 512-byte sectors from external RAM
              at $8000+512*<OFFSET> to SD card starting at sector <SECTOR_NUM>
   After operation is finished set success and error flags at $BFF8 in external
   RAM accordingly.

Atari-side interface

Memory region $BFF8..$BFFF is reserved for SD card read/write control. This
region is not overwritten by SD card read operation.

$BFF8 :
  bit0=1     - operation: read sectors from SD card into RAM
  bit1=1     - operation: write data from RAM to SD card
  bit2=1     - last operation successful
  bit3=1     - last operation failed
  bit7..bit4 - unused

$BFF9 :
  bit4..bit0 - OFFSET; data offset from $8000; measured in 512-byte blocks
  bit7..bit5 - unused

$BFFA :
  bit5..bit0 - <SECTOR_COUNT>; number of 512-byte sectors to read/write
  bit7..bit6 - unused

$BFFB :
  bit7..bit0 - <SECTOR_NUM> bits 7..0; SD card sector number bits 7..0

$BFFC :
  bit7..bit0 - <SECTOR_NUM> bits 15..8; SD card sector number bits 15..8

$BFFD :
  bit7..bit0 - <SECTOR_NUM> bits 23..16; SD card sector number bits 23..16

$BFFE :
  unused

$BFFF :
  unused

Sources

bootstrap/ - Atari bootstrap sources
cpld/      - MMU design files
pcb/       - PCB design files (gEDA) and gerbers
tools/     - programs to control the microcontroller through UART interface
uc/        - microcontroller firmware sources
