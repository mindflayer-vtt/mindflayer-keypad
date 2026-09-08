#pragma once

#include <Protocol.h>

namespace FirmwareUpdate {

void installSignatureVerifier();
void perform(const mindflayer::protocol::UpdateAvailable& update);

} // namespace FirmwareUpdate
