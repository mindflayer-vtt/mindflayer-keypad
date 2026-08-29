/* Experimental rBoot application layout for either 1 MiB flash bank.
 * The image is flashed at 0x002000 (slot A) or 0x202000 (slot B), matching
 * upstream rBoot's default two-ROM layout on 4 MiB flash.
 */
MEMORY
{
  dport0_0_seg : org = 0x3FF00000, len = 0x10
  dram0_0_seg  : org = 0x3FFE8000, len = 0x14000
  irom0_0_seg  : org = 0x40202010, len = 0xFDFF0
}

/* No filesystem is used by the test image. Keep Core 3.1.2's 4 MiB SDK
 * configuration addresses so RF calibration/Wi-Fi sectors remain at the end.
 */
PROVIDE ( _FS_start = 0x405FB000 );
PROVIDE ( _FS_end = 0x405FB000 );
PROVIDE ( _FS_page = 0x0 );
PROVIDE ( _FS_block = 0x0 );
PROVIDE ( _EEPROM_start = 0x405fb000 );
PROVIDE ( _SPIFFS_start = 0x405FB000 );
PROVIDE ( _SPIFFS_end = 0x405FB000 );
PROVIDE ( _SPIFFS_page = 0x0 );
PROVIDE ( _SPIFFS_block = 0x0 );

INCLUDE "local.eagle.app.v6.common.ld"

