#pragma once

namespace SerialProvisioning {

void configureSerial();
bool enterRecoveryModeIfRequested(bool temporaryBoot);
void clearRecoveryMarker();
void poll();

} // namespace SerialProvisioning
