#pragma once
#include <stdint.h>
namespace mindflayer { namespace flashlayout {
constexpr uint32_t FLASH_SIZE=0x400000, SECTOR_SIZE=0x1000, PROVISIONING_A=0x3f9000, PROVISIONING_B=0x3fa000;
constexpr uint32_t CURRENT_OTA_END=0x300000, FUTURE_RBOOT_START=0x000000, FUTURE_RBOOT_END=0x002000;
constexpr uint32_t FUTURE_SLOT_A_START=0x002000, FUTURE_SLOT_A_END=0x100000, FUTURE_SLOT_B_START=0x202000, FUTURE_SLOT_B_END=0x300000;
constexpr uint32_t EEPROM_START=0x3fb000, FRAMEWORK_TAIL_END=0x400000;
static_assert(PROVISIONING_A%SECTOR_SIZE==0 && PROVISIONING_B==PROVISIONING_A+SECTOR_SIZE, "Provisioning sectors must be adjacent and aligned");
static_assert(FUTURE_RBOOT_START==0&&FUTURE_RBOOT_END==FUTURE_SLOT_A_START&&FUTURE_SLOT_A_END<=FUTURE_SLOT_B_START, "Unexpected future rBoot layout");
static_assert(PROVISIONING_A>=CURRENT_OTA_END&&PROVISIONING_A>=FUTURE_SLOT_B_END&&PROVISIONING_B+SECTOR_SIZE<=EEPROM_START, "Provisioning overlaps current OTA, future rBoot, or framework tail");
static_assert(EEPROM_START==0x3fb000 && FRAMEWORK_TAIL_END==FLASH_SIZE, "Unexpected ESP8266 4 MiB framework tail");
} }
