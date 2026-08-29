// Adapted from rBoot appcode/rboot-bigflash.c at pinned commit 614f336.
// Copyright 2015 Richard A Burton; distributed under rBoot's MIT license.
#include <Arduino.h>
#include <stddef.h>

extern "C" {

void Cache_Read_Disable(void);
void Cache_Read_Enable(uint32_t, uint32_t, uint32_t);

struct __attribute__((packed)) RbootRtcData {
  uint32_t magic;
  uint8_t nextMode, lastMode, lastRom, temporaryRom, checksum;
};

uint8_t rBoot_mmap_1 = 0xff;
uint8_t rBoot_mmap_2 = 0xff;

void IRAM_ATTR Cache_Read_Enable_New(void) {
  if (rBoot_mmap_1 == 0xff) {
    const size_t offset = offsetof(RbootRtcData, lastRom);
    volatile uint32_t* word = reinterpret_cast<volatile uint32_t*>(
        0x60001100 + (64 * 4) + (offset & ~3));
    uint32_t value = *word;
    const uint8_t rom = reinterpret_cast<uint8_t*>(&value)[offset & 3];
    value = (rom == 0 ? 0x002000 : 0x202000) / 0x100000;
    rBoot_mmap_2 = value / 2;
    rBoot_mmap_1 = value % 2;
  }
  Cache_Read_Enable(rBoot_mmap_1, rBoot_mmap_2, 1);
}

}  // extern "C"

