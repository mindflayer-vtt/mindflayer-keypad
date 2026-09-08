#include "HealthGate.h"

namespace mindflayer {
namespace health {

bool shouldPromote(const State& s) {
  return s.temporary && s.provisioning && s.normalMode && s.wifi && s.pinnedTls && s.wss &&
         s.hmac && s.registered && s.serverAccepted && s.acceptedVersionMatches;
}

bool shouldArmSerialRecovery(bool temporaryBoot, bool permanentBoot) {
  return permanentBoot && !temporaryBoot;
}

} // namespace health
} // namespace mindflayer
