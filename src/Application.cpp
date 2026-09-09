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
extern "C" {
#include <user_interface.h>
}
#endif

namespace {
namespace KeyboardMatrix = com::viromania::vtt::wss::KeyboardMatrix;
namespace provisioning = mindflayer::provisioning;

void handleRestartShortcut() {
  const auto& keys = *KeyboardMatrix::getState();
  // Physical matrix positions: E, Shift, Space. Check the completed scan rather
  // than acting inside a key callback while the other rows are still stale.
  if (keys[0][2].isDown && keys[3][0].isDown && keys[3][2].isDown) {
    DebugLog::println("Shift+Space+E: restarting");
    ESP.restart();
  }
}

#ifdef RBOOT_INTEGRATION
constexpr uint32_t TEMPORARY_HEALTH_TIMEOUT_MS = 90000;
os_timer_t temporaryHealthTimer;

void restartUnhealthyCandidate(void*) {
  // SDK timers run in SYS context, including during yielding Wi-Fi/TLS waits.
  // Do not log, flush, or call ESP.restart(): its esp_suspend() requires CONT.
  system_restart();
}
#endif
} // namespace

namespace Application {

void setup() {
  ApplicationState& state = applicationState();
  SerialProvisioning::configureSerial();
  FirmwareUpdate::installSignatureVerifier();
#ifdef RBOOT_INTEGRATION
  if (!BootControl::begin()) {
    ESP.restart();
    return;
  }
  state.temporaryBoot = BootControl::isTemporaryBoot();
  state.temporaryStarted = millis();
  os_timer_disarm(&temporaryHealthTimer);
  if (state.temporaryBoot) {
    os_timer_setfn(&temporaryHealthTimer, restartUnhealthyCandidate, nullptr);
    os_timer_arm(&temporaryHealthTimer, TEMPORARY_HEALTH_TIMEOUT_MS, false);
  }
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
#ifdef RBOOT_INTEGRATION
  // Also check before every early-return path in the application loop.
  if (state.temporaryBoot && millis() - state.temporaryStarted >= TEMPORARY_HEALTH_TIMEOUT_MS) {
    ESP.restart();
    return;
  }
#endif
  if (!state.provisioned) {
    SerialProvisioning::poll();
    delay(10);
    return;
  }
  DeviceConnection::poll();
  handleRestartShortcut();
#ifdef RBOOT_INTEGRATION
  const uint32_t temporaryElapsed = millis() - state.temporaryStarted;
  RBootTestHooks::maybeFailBeforeServerAcknowledgement(state.temporaryBoot, temporaryElapsed);
#endif
}

} // namespace Application
