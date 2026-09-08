#include "RBootTestHooks.h"

#include <Arduino.h>

namespace RBootTestHooks {

bool corruptCandidateAfterValidation(RBootSlot::Flash& flash, RBootSlot::Slot slot) {
#ifdef TEST_CORRUPT_CANDIDATE_AFTER_VALIDATION
  // boot2 bytes 0x10 onward are the checksummed IROM payload. Clear one bit
  // without an erase, after application validation but before temporary boot.
  const uint32_t address = RBootSlot::slotStart(slot) + 0x20;
  uint32_t word;
  const bool readable = RBootSlot::validAbsoluteRange(slot, address, sizeof(word)) &&
                        flash.read(address, &word, sizeof(word));
  const bool corrupted =
      readable && word && ((word &= word - 1), flash.write(address, &word, sizeof(word)));
  Serial.println(corrupted ? "TEST: corrupted validated inactive image before temporary boot"
                           : "TEST: unable to corrupt validated inactive image safely");
  return corrupted;
#else
  (void)flash;
  (void)slot;
  return true;
#endif
}

bool withholdCandidateRegistration(bool temporaryBoot) {
#ifdef TEST_FAIL_BEFORE_SERVER_ACK
  if (temporaryBoot) {
    Serial.println("TEST: withholding candidate registration before forced failure");
    return true;
  }
#else
  (void)temporaryBoot;
#endif
  return false;
}

void maybeFailBeforeServerAcknowledgement(bool temporaryBoot, uint32_t elapsedMs) {
#ifdef TEST_FAIL_BEFORE_SERVER_ACK
  if (temporaryBoot && elapsedMs > 3000) {
    Serial.println("TEST: failing temporary candidate before server acknowledgement");
    Serial.flush();
    ESP.restart();
  }
#else
  (void)temporaryBoot;
  (void)elapsedMs;
#endif
}

#ifdef RBOOT_PROMOTION_RESET_STAGE
namespace {
static_assert(RBOOT_PROMOTION_RESET_STAGE <= static_cast<uint8_t>(BootMetadata::Stage::Committed),
              "promotion reset stage is outside the BootMetadata transaction");
uint8_t promotionResetStage = RBOOT_PROMOTION_RESET_STAGE;

void resetAtPromotionStage(BootMetadata::Stage stage, void* context) {
  if (static_cast<uint8_t>(stage) == *static_cast<uint8_t*>(context))
    ESP.restart();
}
} // namespace
#endif

BootMetadata::Hook promotionHook() {
#ifdef RBOOT_PROMOTION_RESET_STAGE
  return resetAtPromotionStage;
#else
  return nullptr;
#endif
}

void* promotionContext() {
#ifdef RBOOT_PROMOTION_RESET_STAGE
  return &promotionResetStage;
#else
  return nullptr;
#endif
}

} // namespace RBootTestHooks
