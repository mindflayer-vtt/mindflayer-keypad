#pragma once

namespace mindflayer {
namespace health {

struct State {
  bool temporary;
  bool provisioning;
  bool normalMode;
  bool wifi;
  bool pinnedTls;
  bool wss;
  bool hmac;
  bool registered;
  bool serverAccepted;
  bool acceptedVersionMatches;
};

bool shouldPromote(const State& state);
bool shouldArmSerialRecovery(bool temporaryBoot, bool permanentBoot);

} // namespace health
} // namespace mindflayer
