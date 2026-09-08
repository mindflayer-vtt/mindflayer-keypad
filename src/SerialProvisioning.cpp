#include "SerialProvisioning.h"

#include <Arduino.h>
#include <HealthGate.h>
#include <Provisioning.h>
#include <ProvisioningStorage.h>
#include <RecoveryMode.h>
#include <stdint.h>

namespace {
namespace provisioning = mindflayer::provisioning;

alignas(4) uint8_t provisioningBuffer[provisioning::MAX_ENVELOPE_SIZE];
size_t provisioningReceived = 0;
size_t provisioningExpected = 0;

bool consumeRecoveryMarker() {
  mindflayer::recovery::Marker marker = {};
  const bool requested =
      ESP.rtcUserMemoryRead(mindflayer::recovery::RTC_OFFSET, (uint32_t*)&marker, sizeof(marker)) &&
      mindflayer::recovery::markerValid(marker);
  SerialProvisioning::clearRecoveryMarker();
  return requested;
}

void armRecoveryMarker() {
  auto marker = mindflayer::recovery::makeMarker();
  ESP.rtcUserMemoryWrite(mindflayer::recovery::RTC_OFFSET, (uint32_t*)&marker, sizeof(marker));
}
} // namespace

namespace SerialProvisioning {

void configureSerial() {
  Serial.setRxBufferSize(provisioning::MAX_ENVELOPE_SIZE + 16);
  Serial.begin(115200);
}

void clearRecoveryMarker() {
  mindflayer::recovery::Marker marker = {};
  ESP.rtcUserMemoryWrite(mindflayer::recovery::RTC_OFFSET, (uint32_t*)&marker, sizeof(marker));
}

bool enterRecoveryModeIfRequested(bool temporaryBoot) {
  const bool shouldArm = mindflayer::health::shouldArmSerialRecovery(temporaryBoot, !temporaryBoot);
  if (shouldArm && consumeRecoveryMarker()) {
    Serial.println("SERIAL PROVISIONING MODE; NeoPixel DMA disabled on GPIO3/RXD0");
    return true;
  }
  if (!shouldArm)
    clearRecoveryMarker();
  else {
    armRecoveryMarker();
    Serial.println("Double-reset recovery window open");
    delay(mindflayer::recovery::WINDOW_MS);
    clearRecoveryMarker();
  }
  return false;
}

void poll() {
  static const uint8_t magic[4] = {'M', 'F', 'P', '1'};
  while (Serial.available()) {
    const uint8_t byte = Serial.read();
    if (provisioningReceived < 4 && byte != magic[provisioningReceived]) {
      provisioningReceived = byte == magic[0] ? 1 : 0;
      provisioningExpected = 0;
      continue;
    }
    if (provisioningReceived >= sizeof(provisioningBuffer)) {
      provisioningReceived = provisioningExpected = 0;
      continue;
    }
    provisioningBuffer[provisioningReceived++] = byte;
    if (provisioningReceived == provisioning::ENVELOPE_HEADER_SIZE) {
      const size_t payload = ((size_t)provisioningBuffer[5] << 8) | provisioningBuffer[6];
      if (payload == 0 || payload > provisioning::MAX_PAYLOAD_SIZE) {
        Serial.println("PROVISIONING ERROR length");
        provisioningReceived = provisioningExpected = 0;
        continue;
      }
      provisioningExpected =
          provisioning::ENVELOPE_HEADER_SIZE + payload + provisioning::ENVELOPE_CRC_SIZE;
    }
    if (provisioningExpected && provisioningReceived == provisioningExpected) {
      if (!provisioning::storeAtomically(provisioningBuffer, provisioningReceived))
        Serial.println("PROVISIONING ERROR invalid envelope or flash store");
      else {
        Serial.println("PROVISIONING OK; rebooting");
        delay(100);
        ESP.restart();
      }
      provisioningReceived = provisioningExpected = 0;
    }
  }
}

} // namespace SerialProvisioning
