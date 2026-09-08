#pragma once

#include <BootMetadata.h>
#include <RBootSlot.h>
#include <stdint.h>

#if (defined(RBOOT_PROMOTION_RESET_STAGE) || defined(TEST_FAIL_BEFORE_SERVER_ACK) ||               \
     defined(TEST_CORRUPT_CANDIDATE_AFTER_VALIDATION)) &&                                          \
    !defined(MINDFLAYER_FAULT_INJECTION_BUILD)
#error "rBoot fault injection requires a dedicated fault-injection build"
#endif

namespace RBootTestHooks {

bool corruptCandidateAfterValidation(RBootSlot::Flash& flash, RBootSlot::Slot slot);
bool withholdCandidateRegistration(bool temporaryBoot);
void maybeFailBeforeServerAcknowledgement(bool temporaryBoot, uint32_t elapsedMs);
BootMetadata::Hook promotionHook();
void* promotionContext();

} // namespace RBootTestHooks
