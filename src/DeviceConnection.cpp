#include "DeviceConnection.h"

#include "ApplicationState.h"
#include "BuildConfig.h"
#include "DebugLog.h"
#include "FirmwareUpdate.h"
#include "LedController.h"

#include <Arduino.h>
#include <HardwareConfig.h>
#include <HealthGate.h>
#include <KeyboardMatrix.h>
#include <Protocol.h>
#ifdef RBOOT_INTEGRATION
#include <BootControl.h>
#include <RBootTestHooks.h>
#endif

namespace {
using namespace websockets;
namespace KeyboardMatrix = com::viromania::vtt::wss::KeyboardMatrix;
namespace protocol = mindflayer::protocol;

uint8_t frameBuffer[protocol::MAX_DEVICE_FRAME_SIZE];
uint8_t pendingFrame[protocol::MAX_DEVICE_FRAME_SIZE];
size_t pendingFrameSize = 0;
uint8_t receivedFrame[protocol::MAX_DEVICE_FRAME_SIZE];
size_t receivedFrameSize = 0;
bool receivedFrameInvalid = false;
bool eventOpened = false, eventClosed = false, eventPing = false, eventPong = false;
bool reconnectRequested = false;
uint32_t lastPong = 0;

void sendFrame(size_t size) {
  if (size && size <= sizeof(frameBuffer) && !pendingFrameSize) {
    memcpy(pendingFrame, frameBuffer, size);
    pendingFrameSize = size;
  }
}

void flushFrame() {
  ApplicationState& state = applicationState();
  if (pendingFrameSize && state.client.available()) {
    const size_t size = pendingFrameSize;
    pendingFrameSize = 0;
    state.client.sendBinary((const char*)pendingFrame, size);
  }
}

void processMessage(const uint8_t* data, size_t size) {
  ApplicationState& state = applicationState();
  lastPong = millis();
  protocol::AuthChallenge challenge;
  size_t written;
  if (protocol::parseAuthChallenge(data, size, challenge)) {
    if (protocol::buildAuthResponse(frameBuffer, sizeof(frameBuffer), written,
                                    state.settings.deviceId, state.settings.deviceSecret,
                                    challenge.challenge))
      sendFrame(written);
    return;
  }
  protocol::AuthResult auth;
  if (protocol::parseAuthResult(data, size, auth)) {
    if (auth.status != protocol::AUTH_OK || strcmp(auth.deviceId, state.settings.deviceId)) {
      state.client.close(CloseReason_PolicyViolation);
      return;
    }
    state.authenticated = true;
    bool withholdRegistration = false;
#ifdef RBOOT_INTEGRATION
    withholdRegistration = RBootTestHooks::withholdCandidateRegistration(state.temporaryBoot);
#endif
    if (!withholdRegistration &&
        protocol::buildRegistration(frameBuffer, sizeof(frameBuffer), written, FIRMWARE_VERSION,
                                    HARDWARE_ID)) {
      sendFrame(written);
      state.registered = true;
    }
    DebugLog::printf("Authenticated as %s; firmware=%s; ", state.settings.deviceId,
                     FIRMWARE_VERSION);
    printHeapStats();
    return;
  }
  protocol::UpdateAvailable update;
  if (state.authenticated && protocol::parseUpdateAvailable(data, size, update)) {
    state.client.close(CloseReason_GoingAway);
    state.client = WebsocketsClient();
    state.authenticated = state.wssHealthy = state.registered = false;
    DebugLog::print("WSS released for signed OTA; ");
    printHeapStats();
    FirmwareUpdate::perform(update);
    reconnectRequested = true;
    return;
  }
  protocol::FirmwareAccepted accepted;
  if (state.authenticated && protocol::parseFirmwareAccepted(data, size, accepted)) {
#ifdef RBOOT_INTEGRATION
    const mindflayer::health::State health = {state.temporaryBoot,
                                              state.provisioned,
                                              true,
                                              state.wifiHealthy,
                                              state.serverPublicKey != nullptr,
                                              state.wssHealthy,
                                              state.authenticated,
                                              state.registered,
                                              true,
                                              !strcmp(accepted.version, FIRMWARE_VERSION)};
    if (mindflayer::health::shouldPromote(health)) {
      DebugLog::println("Server accepted candidate; promoting transactionally");
      if (BootControl::promoteCurrentSlot(RBootTestHooks::promotionHook(),
                                          RBootTestHooks::promotionContext())) {
        DebugLog::flush();
        ESP.restart();
      } else
        DebugLog::println("Candidate promotion failed");
    }
#endif
    return;
  }
  protocol::Configuration configuration;
  if (state.authenticated && protocol::parseConfiguration(data, size, configuration)) {
    LedController::setColors(configuration.led1.r, configuration.led1.g, configuration.led1.b,
                             configuration.led2.r, configuration.led2.g, configuration.led2.b);
    return;
  }
  DebugLog::println("Rejected malformed or unauthorized CBOR message");
  state.client.close(CloseReason_ProtocolError);
}

void onMessage(WebsocketsMessage message) {
  const size_t size = message.length();
  if (!message.isBinary() || !size || size > sizeof(receivedFrame) || receivedFrameSize) {
    receivedFrameInvalid = true;
    return;
  }
  memcpy(receivedFrame, message.c_str(), size);
  receivedFrameSize = size;
}

void onEvent(WebsocketsEvent event, String) {
  if (event == WebsocketsEvent::ConnectionOpened)
    eventOpened = true;
  else if (event == WebsocketsEvent::ConnectionClosed)
    eventClosed = true;
  else if (event == WebsocketsEvent::GotPing)
    eventPing = true;
  else if (event == WebsocketsEvent::GotPong)
    eventPong = true;
}

void processCallbacks() {
  ApplicationState& state = applicationState();
  if (eventOpened) {
    eventOpened = false;
    state.authenticated = false;
    state.wssHealthy = true;
    DebugLog::print("Pinned binary WSS connected; ");
    printHeapStats();
  }
  if (eventClosed) {
    eventClosed = false;
    state.authenticated = false;
    state.wssHealthy = state.registered = false;
    reconnectRequested = true;
    DebugLog::println("WSS closed; reconnecting");
  }
  if (eventPing) {
    eventPing = false;
    state.client.pong();
  }
  if (eventPong) {
    eventPong = false;
    lastPong = millis();
  }
  if (receivedFrameInvalid) {
    receivedFrameInvalid = false;
    receivedFrameSize = 0;
    DebugLog::println("Rejected non-binary, oversized, or overlapping device frame");
    state.client.close(CloseReason_ProtocolError);
    return;
  }
  if (receivedFrameSize) {
    const size_t size = receivedFrameSize;
    receivedFrameSize = 0;
    processMessage(receivedFrame, size);
  }
}

void connect() {
  ApplicationState& state = applicationState();
  state.client.onMessage(onMessage);
  state.client.onEvent(onEvent);
  state.client.setKnownKey(state.serverPublicKey);
  if (!state.client.connectSecure(state.settings.serverHost, state.settings.serverPort,
                                  "/device/v1")) {
    DebugLog::println("Pinned WSS connection failed");
    reconnectRequested = true;
    return;
  }
  lastPong = millis();
}

void onKeyChange(KeyboardMatrix::KeyState* key) {
  ApplicationState& state = applicationState();
  size_t written;
  if (protocol::buildKeyEvent(frameBuffer, sizeof(frameBuffer), written, key->key, key->isDown))
    state.client.sendBinary((const char*)frameBuffer, written);
}
} // namespace

namespace DeviceConnection {

void begin() { connect(); }

void poll() {
  ApplicationState& state = applicationState();
  state.client.poll();
  processCallbacks();
  flushFrame();
  if (state.authenticated && state.client.available())
    KeyboardMatrix::detectKeys(onKeyChange);
  if (reconnectRequested) {
    reconnectRequested = false;
    delay(500);
    state.client = WebsocketsClient();
    connect();
  }
  if (state.client.available() && millis() - lastPong > 15000) {
    state.client.ping();
    lastPong = millis();
  }
}

} // namespace DeviceConnection
