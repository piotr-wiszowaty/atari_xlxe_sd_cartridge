Atari XL/XE SD cartridge
========================

Atari XL/XE SD cartridge is a device allowing access (read and write) to FAT32 formatted
SD cards by [Atari 8-bit computers](http://en.wikipedia.org/wiki/Atari_8-bit_family).

Atari-side interface
--------------------

The cartridge occupies address space `$8000..$BFFF`. Read and write operations
are controlled through 8 registers mapped to `$D5E8..$D5EF`. Each read/write
operates on SD card sectors `<SECTOR_NUM>..<SECTOR_NUM>+<SECTOR_COUNT>-1` and
RAM region `$8000+512*<OFFSET>..$8000+512*(<OFFSET>+<SECTOR_COUNT>)-1`.

`$D5E8` :

* bit0=1     - read sectors from SD card into buffer RAM; reset after the operation is completed
* bit1=1     - write data from buffer RAM to SD card; reset after the operation is completed
* bit5..bit2 - unused
* bit6=1     - flag: last operation succeeded
* bit7=1     - flag: last operation failed

`$D5E9` :

* bit4..bit0 - `<OFFSET>`; data offset from `$8000`; measured in 512-byte blocks
* bit7..bit5 - unused

`$D5EA` :

* bit5..bit0 - `<SECTOR_COUNT>`; number of 512-byte sectors to read/write
* bit7..bit6 - unused

`$D5EB` :

* bit7..bit0 - `<SECTOR_NUM>` bits 7..0

`$D5EC` :

* bit7..bit0 - `<SECTOR_NUM>` bits 15..8

`$D5ED` :

* bit7..bit0 - `<SECTOR_NUM>` bits 23..16

`$D5EE` :

* bit7..bit0 - `<SECTOR_NUM>` bits 31..24

`$D5EF` :

* unused

Example usage
-------------

### Read

<pre><code>
 ; read 10 consecutive sectors from SD card starting from $00123456
 ; into buffer RAM starting at $8600 and wait for the
 ; operation to complete
 lda #$56
 sta $D5EB
 lda #$34
 sta $D5EC
 lda #$12
 sta $D5ED
 lda #$00
 sta $D5EE
 lda #3
 sta $D5E9
 lda #10
 sta $D5EA
 lda #1
 sta $D5E8
 lda #$C0
wait
 bit $D5E8
 beq wait
 bmi error
ok
</code></pre>

### Write

<pre><code>
 ; fill 7 consecutive sectors on SD card starting from $00987654
 ; with data in buffer RAM starting at $a000 and wait for the
 ; operation to complete
 lda #$54
 sta $D5EB
 lda #$76
 sta $D5EC
 lda #$98
 sta $D5ED
 lda #$00
 sta $D5EE
 lda #16
 sta $D5E9
 lda #7
 sta $D5EA
 lda #2
 sta $D5E8
 lda #$C0
wait
 bit $D5E8
 beq wait
 bmi error
ok
</code></pre>

Hardware
--------

Main silicon components:

* buffer RAM - 32 kB of 55 ns SRAM (16 kB + 256 B of which are actually used)
* ARM Cortex-M0 microcontroller (NXP LPC1114)
* MMU controlling access to SRAM from microcontroller and Atari (Xilinx XC95144XL CPLD)

Connectors:

* SD card socket
* SWD connector for programming/debugging the microcontroller
* JTAG connector for programming the CPLD
* UART connector (5V) for communication PC-microcontroller; may also be used for microcontroller flash programming
* debug connector with two pins connected to the CPLD, and two pins connected to both the CPLD and the microcontroller
* SPI header (for debugging communication between the microcontroller and the SD card)

LED indicators:

* red : error occured (e.g. SD card not present, SD card read or write error)
* yellow : SD card present

Software (microcontroller)
--------------------------

Boot algorithm:

1. Write Atari bootstrap code to buffer RAM at `$BF00`
2. Initialize SD card
3. Find Atari executable file boot.xex on the SD card
4. Write boot.xex first sector number to buffer RAM at `$D5EB..$D5ED`
5. Execute main loop

Main loop:

1. Execute command received through UART (if any):
   * echo
     UART --> microcontroller `'e'`
     UART <-- microcontroller `'e'`

   * read data from buffer RAM (`A`, `N` - 16-bit little endian numbers)
     UART --> microcontroller `'r'`
     UART --> microcontroller `A`
     UART --> microcontroller `N`
     UART <-- microcontroller `buffer_ram[A]`
     UART <-- microcontroller `buffer_ram[A+1]`
     ...
     UART <-- microcontroller `buffer_ram[A+N-1]`

   * write data to buffer RAM (`A`, `N` - 16-bit little endian numbers)
     UART --> microcontroller `'w'`
     UART --> microcontroller `A`
     UART --> microcontroller `N`
     UART --> microcontroller `buffer_ram[A]`
     UART --> microcontroller `buffer_ram[A+1]`
     ...
     UART --> microcontroller `buffer_ram[A+N-1]`
     UART <-- microcontroller `'w'`

2. Execute operation read from register at `$D5E8`:
   * bit0=1 - read `<SECTOR_COUNT>` 512-byte sectors starting at sector `<SECTOR_NUM>`
              from SD card into buffer RAM at address `$8000+512*<OFFSET>`
   * bit1=1 - write `<SECTOR_COUNT>` 512-byte sectors from buffer RAM
              at `$8000+512*<OFFSET>` to SD card starting at sector `<SECTOR_NUM>`
   After operation is finished set success and error flags in register at `$D5E8`.

Sources
-------

`bootstrap/` - Atari bootstrap sources

`cpld/`      - MMU design files

`pcb/`       - PCB design files (gEDA) and gerbers

`tools/`     - programs to control the microcontroller through UART interface

`uc/`        - microcontroller firmware sources

`video/`     - video converter
