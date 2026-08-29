/* 4 MiB physical flash, one normal eboot/OTA sketch region, no filesystem. */
/* sketch 0x001000..0x0fffff (~1019 KiB) */
/* OTA staging is bounded below the empty filesystem boundary at 0x300000. */
/* provisioning 0x3f9000..0x3fafff is owned by ProvisioningStore. */
/* eeprom/rfcal/wifi 0x3fb000..0x3fffff remain framework-owned. */

MEMORY
{
  dport0_0_seg : org = 0x3FF00000, len = 0x10
  dram0_0_seg : org = 0x3FFE8000, len = 0x14000
  irom0_0_seg : org = 0x40201010, len = 0xfeff0
}

/* Preserve the framework's boundary symbols while configuring zero FS bytes. */
PROVIDE ( _FS_start = 0x40500000 );
PROVIDE ( _FS_end = 0x40500000 );
PROVIDE ( _FS_page = 0x100 );
PROVIDE ( _FS_block = 0x1000 );
PROVIDE ( _EEPROM_start = 0x405fb000 );
PROVIDE ( _SPIFFS_start = 0x40500000 );
PROVIDE ( _SPIFFS_end = 0x40500000 );
PROVIDE ( _SPIFFS_page = 0x100 );
PROVIDE ( _SPIFFS_block = 0x1000 );

INCLUDE "local.eagle.app.v6.common.ld"
