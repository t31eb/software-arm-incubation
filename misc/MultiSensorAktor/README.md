Multi-Sensor-Aktor
==================

Generic Firmware for LPC115-based controller, like TS-ARM or 4TE-Controller.
Features:
<ul>
<li> Each available pin can be independently used as:
 <ul>
   <li> Input
     <ul>
       <li> with pullup, pulldown or floating</li>
       <li> inverted or not</li>
       <li> Support for long keypress detection with separate action</li>
       <li> Input debouncer</li>
       <li> Actions on Hi/Lo: On, Off, Toggle or nothing</li>
     </ul>
   </li>
  <li> Output
    <ul>
      <li> inverted or not</li>
      <li> blinking support, on 1, on 0, on separate object</li>
      <li> blink phase on/off configurable (don't use to low number, this is no PWM!)</li>
    </ul>
   </li>
  <li> DHT11/DHT22
    <ul>
      <li> Temperature as DPT9 (16bit) and DTP14 (32bit) float</li>
      <li> Temperature offset configurable</li>
      <li> optional Fan-phase before measurement for bad ventilated environments</li>
      <li> configurable measurement interval</li>
      <li> Humidity as DPT9</li>
    </ul>
   </li>
  <li> PWM
    <ul>
      <li> 0-100% via ComObject</li>
      <li> inverted or not</li>
      <li> fixed 100Hz</li>
    </ul>
   </li>
  </ul>
 </li>
</ul>
  
Work in progress:
<ul>
<li> Support for sensors/actors on I2C, SPI
  <ul>
    <li> PCA955D 16 Bit IO extender, Input/Output/PWM as on ARM-pins</li>
    <li> CCS811</li>
    <li> BH1750</li>
    <li> DS3231</li>
    <li> iAQ-Core</li>
    <li> SHT2x</li>
    <li> radio based sensors via RFM69</li>
  </ul>
</li>
</ul>

Knxprod:
the contents of the folder KnxProd (not the folder itself) have to zipped to MultiSensorAktor.knxprod. This file can be signed with the KnxProdEditor (https://github.com/knxprodeditor/KNXprodEditor), just open and save. Do not edit in the editor, it doesn't support all necessary features! The signed knxprod can be imported into ETS5 & 6 (maybe also 4)

## Building with CMake

Without MCUXpresso, e.g. on Linux with Debian's `gcc-arm-none-eabi` (see `cmake/toolchains/README.md` of the sblib):

```sh
cmake --preset arm -DCMAKE_TOOLCHAIN_FILE=<sblib repository>/cmake/toolchains/arm-none-eabi_gnu.cmake -DSB_PATH_TO_SBLIB_REPO=<sblib repository>
cmake --build --preset release-arm
```

The result in `cmake-build/arm/Release` starts at 0x3000 like the configuration `Flashstart 0x3000 Release`.
`linkscripts/MultiSensorAktor_Flashstart_0x3000_combined.ld` ends the flash at 0xE000, where the configuration in the
flash starts (see below). Only the Release build fits, the Debug build (-O0) needs about 72 KB and fails at link time.

`-DMSA_FLASH_START=0x0` links the application to 0x0000 instead, for a device without bootloader that is
flashed via SWD (e.g. a test bench). The linker script is then generated from the 0x3000 one in the build directory.
Such a build must not be loaded through the bootloader.

## Unit tests

`test/` builds the unit tests on the host (x64) against the CPU emulation and Catch of the sblib repository,
see `test/README.md`.

## Configuration memory

ETS writes two ranges: the user EEPROM of mask 0701 (0x3F00-0x4AFF, 3072 bytes: tables, load states, the
configuration of the ARM pins from 0x4800) and the segment `AS-6000` of the product database (0x6000-0x6EFF,
3840 bytes: the configuration of the I2C items). `-DMSA_USER_EEPROM=` chooses where both are kept:

| | `flash` (default) | `M95512R` / `MB85RS256B` |
| --- | --- | --- |
| user EEPROM | flash 0xF000-0xFBFF (last sector), valid if 0xFBFF isn't 0xff | chip: header 0x0000, data 0x0080-0x0C7F |
| 0x6000-0x6EFF | flash 0xE100-0xEFFF | chip: header 0x1000, data 0x1080-0x1F7F |
| RAM | one page of 256 bytes, shared by both ranges | none |
| application | 0x3000 (0x0 without bootloader) to 0xE000 | to 0x10000 |

Flash, 64 KB in sectors of 4 KB and pages of 256 bytes:

| address | `flash` | `M95512R` / `MB85RS256B` |
| --- | --- | --- |
| 0x0000-0x2FFF | bootloader | bootloader |
| 0x3000-0xDFFF | application | application |
| 0xE000-0xE0FF | allocation table of the MemMapper of earlier versions, no longer written | application |
| 0xE100-0xEFFF | 0x6000-0x6EFF, in order | application |
| 0xF000-0xFBFF | user EEPROM | application |
| 0xFC00-0xFFFF | unused | application |

The flash variant keeps both ranges where earlier versions did (`UserEepromMirror` and the MemMapper with its pages
in order behind the allocation table), so a firmware update needs no new download. Writing a page back erases it
with interrupts disabled for about 100 ms; the bus pauses meanwhile. In the chip every write goes to the chip right
away, a header with a CRC marks each range as valid; an empty chip is formatted (zeros) at the start.

The configuration doesn't move between flash and chip on its own: after changing `MSA_USER_EEPROM` the device needs a
new download with ETS. Going back from the chip to the flash, erase the flash from 0xE000 first (e.g. the full erase of
the updater), an application that reached into it would otherwise be read as configuration.

