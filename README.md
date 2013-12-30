Atari XL/XE SD cartridge
========================

Atari XL/XE SD cartridge is a device allowing access (read and write) to SD
cards by [Atari 8-bit computers](http://en.wikipedia.org/wiki/Atari_8-bit_family).

Access is performed through buffer RAM accessible at address range
`$8000..$bfff` and 8 control registers at `$d5e8..$d5ef`. SD card communication
is done by a microcontroller. Access to the buffer RAM by Atari and the
microcontroller is arbitered by a dedicated MMU.

Atari-side interface
--------------------

Memory region `$8000..$bfff` is seen from the Atari as regular RAM - both
readable and writable by Atari. Every read/write operates on SD card sectors
`<SECTOR_NUM>..<SECTOR_NUM>+<SECTOR_COUNT>-1` and RAM region 
`$8000+512*<OFFSET>..$8000+512*(<OFFSET>+<SECTOR_COUNT>)-1`.

`$d5e8` :

* bit0=1     - read sectors from SD card into buffer RAM; reset by microcontroller after read is completed
* bit1=1     - write data from buffer RAM to SD card; reset by microcontroller after write is completed
* bit2=1     - flag: last operation succeeded
* bit3=1     - flag: last operation failed
* bit7..bit4 - unused

`$d5e9` :

* bit4..bit0 - `<OFFSET>`; data offset from `$8000`; measured in 512-byte blocks
* bit7..bit5 - unused

`$d5ea` :

* bit5..bit0 - `<SECTOR_COUNT>`; number of 512-byte sectors to read/write
* bit7..bit6 - unused

`$d5eb` :

* bit7..bit0 - `<SECTOR_NUM>` bits 7..0

`$d5ec` :

* bit7..bit0 - `<SECTOR_NUM>` bits 15..8

`$d5ed` :

* bit7..bit0 - `<SECTOR_NUM>` bits 23..16

`$d5ee` :

* unused

`$d5ef` :

* unused

Example usage
-------------

### Read

<pre><code>
 ; read 10 consecutive sectors from SD card starting from $123456
 ; into buffer RAM starting at $8600 and wait for the
 ; operation to complete
 lda #$56
 sta $d5eb
 lda #$34
 sta $d5ec
 lda #$12
 sta $d5ed
 lda #3
 sta $d5e9
 lda #10
 sta $d5ea
 lda #1
 sta $d5e8
 lda #$0c
wait
 lda #$04
 bit $d5e8
 bne ok
 lda #$08
 bit $d5e8
 bne error
 jmp wait
</code></pre>

### Write

<pre><code>
 ; fill 7 consecutive sectors on SD card starting from $987654
 ; with data in buffer RAM starting at $a000 and wait for the
 ; operation to complete
 lda #$54
 sta $d5eb
 lda #$76
 sta $d5ec
 lda #$98
 sta $d5ed
 lda #16
 sta $d5e9
 lda #7
 sta $d5ea
 lda #2
 sta $d5e8
wait
 lda #$04
 bit $d5e8
 bne ok
 lda #$08
 bit $d5e8
 bne error
 jmp wait
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

1. Write & verify Atari bootstrap code to buffer RAM at `$bf00`
2. Initialize SD card
3. Find Atari executable file boot.xex on the SD card
4. Wait for the Atari to set bytes `$d5e8` to `$a0` and `$d5e9` to `$a5`
5. Write boot.xex first sector number to buffer RAM at `$d5eb..$d5ed`
6. Change bootstrap instruction in buffer RAM at `$bf0d` from BCC to BCS

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

2. Execute operation read from register at `$d5e8`:
   * bit0=1 - read `<SECTOR_COUNT>` 512-byte sectors starting at sector `<SECTOR_NUM>`
              from SD card into buffer RAM at address `$8000+512*<OFFSET>`
   * bit1=1 - write `<SECTOR_COUNT>` 512-byte sectors from buffer RAM
              at `$8000+512*<OFFSET>` to SD card starting at sector `<SECTOR_NUM>`
   After operation is finished set success and error flags in register at `$d5e8`.

Sources
-------

`bootstrap/` - Atari bootstrap sources

`cpld/`      - MMU design files

`pcb/`       - PCB design files (gEDA) and gerbers

`tools/`     - programs to control the microcontroller through UART interface

`uc/`        - microcontroller firmware sources

