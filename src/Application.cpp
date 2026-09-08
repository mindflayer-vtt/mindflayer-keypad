#include "Application.h"

#include "ApplicationState.h"
#include "BuildConfig.h"
#include "DebugLog.h"
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
  FirmwareUpdate::installSignatureVerifier();
#ifdef RBOOT_INTEGRATION
  if (!BootControl::begin()) {
    return;
  }
  state.temporaryBoot = BootControl::isTemporaryBoot();
  state.temporaryStarted = millis();
#endif
  provisioning::Selection selected;
  if (!provisioning::loadStored(state.settings, &selected)) {
    SerialProvisioning::clearRecoveryMarker();
    return;
  }
  DebugLog::setEnabled(state.settings.serialDebug);
  DebugLog::println("");
  DebugLog::printf("Mind Flayer %s booting; flash-real=%lu; flash-configured=%lu; ",
                   FIRMWARE_VERSION, (unsigned long)ESP.getFlashChipRealSize(),
                   (unsigned long)ESP.getFlashChipSize());
  printHeapStats();
#ifdef RBOOT_INTEGRATION
  DebugLog::printf("rBoot slot=%c permanent=%c mode=%s\n", BootControl::currentSlot() ? 'B' : 'A',
                   BootControl::permanentSlot() ? 'B' : 'A',
                   state.temporaryBoot ? "TEMPORARY" : "PERMANENT");
#endif
  DebugLog::printf("Provisioning copy %c generation %lu loaded; ",
                   selected.copy == provisioning::COPY_A ? 'A' : 'B',
                   (unsigned long)selected.generation);
  printHeapStats();
  state.serverPublicKey =
      new BearSSL::PublicKey(state.settings.serverPublicKey, state.settings.serverPublicKeyLength);
  if (!state.serverPublicKey->isRSA() && !state.serverPublicKey->isEC()) {
    DebugLog::println("UNPROVISIONED; invalid server public key");
    return;
  }
  if (SerialProvisioning::enterRecoveryModeIfRequested(state.temporaryBoot))
    return;
  LedController::begin();
  LedController::setColors(255, 0, 0, 0, 0, 0);
  DebugLog::println("NeoPixel DMA active on physical GPIO3/RXD0; serial RX disabled");
  state.provisioned = true;
  WiFi.hostname(state.settings.deviceId);
  WiFi.begin(state.settings.ssid, state.settings.wifiPassword);
  DebugLog::print("Connecting to provisioned Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    DebugLog::print(".");
    delay(500);
  }
  state.wifiHealthy = true;
  DebugLog::printf(" connected: %s; ", WiFi.localIP().toString().c_str());
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
    DebugLog::println("Temporary candidate health timeout; rebooting for rollback");
    DebugLog::flush();
    ESP.restart();
  }
  RBootTestHooks::maybeFailBeforeServerAcknowledgement(state.temporaryBoot, temporaryElapsed);
#endif
}

} // namespace Application
