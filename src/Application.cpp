#include "Application.h"

#include "ApplicationState.h"
#include "BuildConfig.h"
#include "DeviceConnection.h"
#include "FirmwareUpdate.h"
#include "LedController.h"
#include "SerialProvisioning.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <KeyboardMatrix.h>
#include <ProvisioningStorage.h>
#ifdef RBOOT_INTEGRATION
#include <BootControl.h>
#include <RBootTestHooks.h>
#endif

namespace {
namespace KeyboardMatrix = com::viromania::vtt::wss::KeyboardMatrix;
namespace provisioning = mindflayer::provisioning;
#ifdef RBOOT_INTEGRATION
constexpr uint32_t TEMPORARY_HEALTH_TIMEOUT_MS = 90000;
#endif
} // namespace

namespace Application {

void setup() {
  ApplicationState& state = applicationState();
  SerialProvisioning::configureSerial();
  Serial.println();
  Serial.printf("Mind Flayer %s booting; flash-real=%lu; flash-configured=%lu; ", FIRMWARE_VERSION,
                (unsigned long)ESP.getFlashChipRealSize(), (unsigned long)ESP.getFlashChipSize());
  printHeapStats();
  FirmwareUpdate::installSignatureVerifier();
#ifdef RBOOT_INTEGRATION
  if (!BootControl::begin()) {
    Serial.println("FATAL: invalid rBoot metadata/RTC state");
    return;
  }
  state.temporaryBoot = BootControl::isTemporaryBoot();
  state.temporaryStarted = millis();
  Serial.printf("rBoot slot=%c permanent=%c mode=%s\n", BootControl::currentSlot() ? 'B' : 'A',
                BootControl::permanentSlot() ? 'B' : 'A',
                state.temporaryBoot ? "TEMPORARY" : "PERMANENT");
#endif
  provisioning::Selection selected;
  if (!provisioning::loadStored(state.settings, &selected)) {
    SerialProvisioning::clearRecoveryMarker();
    Serial.println(
        "UNPROVISIONED; NeoPixel DMA disabled; awaiting MFP1 serial provisioning envelope");
    return;
  }
  Serial.printf("Provisioning copy %c generation %lu loaded; ",
                selected.copy == provisioning::COPY_A ? 'A' : 'B',
                (unsigned long)selected.generation);
  printHeapStats();
  state.serverPublicKey =
      new BearSSL::PublicKey(state.settings.serverPublicKey, state.settings.serverPublicKeyLength);
  if (!state.serverPublicKey->isRSA() && !state.serverPublicKey->isEC()) {
    Serial.println("UNPROVISIONED; invalid server public key");
    return;
  }
  if (SerialProvisioning::enterRecoveryModeIfRequested(state.temporaryBoot))
    return;
  LedController::begin();
  LedController::setColors(255, 0, 0, 0, 0, 0);
  Serial.println("NeoPixel DMA active on physical GPIO3/RXD0; serial RX disabled");
  state.provisioned = true;
  WiFi.hostname(state.settings.deviceId);
  WiFi.begin(state.settings.ssid, state.settings.wifiPassword);
  Serial.print("Connecting to provisioned Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  state.wifiHealthy = true;
  Serial.printf(" connected: %s; ", WiFi.localIP().toString().c_str());
  printHeapStats();
  KeyboardMatrix::initMatrix();
  DeviceConnection::begin();
}

void loop() {
  ApplicationState& state = applicationState();
  if (!state.provisioned) {
    SerialProvisioning::poll();
    delay(10);
    return;
  }
  DeviceConnection::poll();
#ifdef RBOOT_INTEGRATION
  const uint32_t temporaryElapsed = millis() - state.temporaryStarted;
  if (state.temporaryBoot && temporaryElapsed > TEMPORARY_HEALTH_TIMEOUT_MS) {
    Serial.println("Temporary candidate health timeout; rebooting for rollback");
    Serial.flush();
    ESP.restart();
  }
  RBootTestHooks::maybeFailBeforeServerAcknowledgement(state.temporaryBoot, temporaryElapsed);
#endif
}

} // namespace Application
