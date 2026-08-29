#pragma once

#include <stdint.h>
#include <RBootSlot.h>

namespace BootControl {

constexpr uint8_t kSlotA = static_cast<uint8_t>(RBootSlot::Slot::A);
constexpr uint8_t kSlotB = static_cast<uint8_t>(RBootSlot::Slot::B);
constexpr uint32_t kSlotAAddress = RBootSlot::kSlotAAddress;
constexpr uint32_t kSlotBAddress = RBootSlot::kSlotBAddress;

bool begin();
uint8_t currentSlot();
uint8_t permanentSlot();
bool isTemporaryBoot();
bool bootTemporary(uint8_t slot);
bool promoteCurrentSlot();
#ifdef RBOOT_FAULT_INJECTION
bool promoteCurrentSlotWithReset(uint8_t stage);
#endif

}  // namespace BootControl

