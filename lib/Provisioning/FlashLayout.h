#pragma once
#include <stdint.h>
#include <FlashLayoutValues.h>
namespace mindflayer { namespace flashlayout {
constexpr uint32_t FLASH_SIZE=MF_FLASH_SIZE, SECTOR_SIZE=MF_SECTOR_SIZE;
constexpr uint32_t RBOOT_START=MF_RBOOT_START, RBOOT_END=MF_BOOT_METADATA_A;
constexpr uint32_t BOOT_METADATA_A=MF_BOOT_METADATA_A;
constexpr uint32_t SLOT_A_START=MF_SLOT_A_START, SLOT_A_END=MF_SLOT_A_END;
constexpr uint32_t BOOT_METADATA_B=MF_BOOT_METADATA_B;
constexpr uint32_t SLOT_B_START=MF_SLOT_B_START, SLOT_B_END=MF_SLOT_B_END;
constexpr uint32_t SLOT_SIZE=0x0fe000;
constexpr uint32_t PROVISIONING_A=MF_PROVISIONING_A, PROVISIONING_B=MF_PROVISIONING_B;
constexpr uint32_t EEPROM_START=MF_EEPROM_START, FRAMEWORK_TAIL_END=MF_FLASH_SIZE;
static_assert(PROVISIONING_A%SECTOR_SIZE==0 && PROVISIONING_B==PROVISIONING_A+SECTOR_SIZE, "Provisioning sectors must be adjacent and aligned");
static_assert(RBOOT_END==BOOT_METADATA_A && BOOT_METADATA_A+SECTOR_SIZE==SLOT_A_START, "Boot prefix layout drift");
static_assert(SLOT_A_END-SLOT_A_START==SLOT_SIZE && SLOT_B_END-SLOT_B_START==SLOT_SIZE, "Slot capacity drift");
static_assert(SLOT_A_END<=BOOT_METADATA_B && BOOT_METADATA_B+SECTOR_SIZE<=SLOT_B_START, "Metadata B overlaps a slot");
static_assert(SLOT_B_END<=PROVISIONING_A && PROVISIONING_B+SECTOR_SIZE<=EEPROM_START, "Slots/provisioning/framework overlap");
static_assert(EEPROM_START==0x3fb000 && FRAMEWORK_TAIL_END==FLASH_SIZE, "Unexpected ESP8266 4 MiB framework tail");
} }
