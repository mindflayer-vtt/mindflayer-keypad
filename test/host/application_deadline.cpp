// Execute the real application orchestrator with simulated SDK time and I/O.
#include <Application.h>
#include <ApplicationState.h>
#include <BootControl.h>
#include <DebugLog.h>
#include <KeyboardMatrix.h>
#include <ProvisioningStorage.h>
#include <cassert>
#include <cstdio>
extern "C" {
#include <user_interface.h>
}

bool wifiConnected = true, publicKeyValid = true;
namespace {
uint32_t now = 0;
os_timer_t* sdkTimer = nullptr;
bool temporary = true, stored = true;
uint32_t connectDelay = 0, pollDelay = 0;
ApplicationState state;
struct Restart {};
} // namespace

extern "C" {
void os_timer_disarm(os_timer_t* timer) { timer->armed = false; }
void os_timer_setfn(os_timer_t* timer, os_timer_func_t* callback, void* argument) {
  timer->callback = callback;
  timer->argument = argument;
}
void os_timer_arm(os_timer_t* timer, uint32_t milliseconds, bool repeat) {
  assert(!repeat);
  timer->deadline = now + milliseconds;
  timer->armed = true;
  sdkTimer = timer;
}
void system_restart() { throw Restart{}; }
}
unsigned long millis() { return now; }
void delay(unsigned long milliseconds) {
  if (sdkTimer && sdkTimer->armed && milliseconds >= uint32_t(sdkTimer->deadline - now)) {
    now = sdkTimer->deadline;
    sdkTimer->armed = false;
    sdkTimer->callback(sdkTimer->argument);
  }
  now += milliseconds;
  // Keep the broken pre-fix startup loop from hanging the regression runner.
  if (now > 120000)
    throw "rollback did not occur";
}
void FakeEsp::restart() { system_restart(); }
ApplicationState& applicationState() { return state; }
void printHeapStats() {}
namespace BootControl {
bool begin() { return true; }
bool isTemporaryBoot() { return temporary; }
uint8_t currentSlot() { return temporary ? 1 : 0; }
uint8_t permanentSlot() { return 0; }
} // namespace BootControl
namespace mindflayer::provisioning {
bool loadStored(Provisioning&, Selection* selection) {
  *selection = {COPY_A, 1};
  return stored;
}
} // namespace mindflayer::provisioning
namespace DebugLog {
void setEnabled(bool) {}
bool isEnabled() { return false; }
void print(const char*) {}
void println(const char*) {}
void flush() {}
} // namespace DebugLog
namespace FirmwareUpdate {
void installSignatureVerifier() {}
} // namespace FirmwareUpdate
namespace SerialProvisioning {
void configureSerial() {}
void clearRecoveryMarker() {}
bool enterRecoveryModeIfRequested(bool) { return false; }
void poll() {}
} // namespace SerialProvisioning
namespace LedController {
void begin() {}
void setColors(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) {}
} // namespace LedController
namespace com::viromania::vtt::wss::KeyboardMatrix {
void initMatrix() {}
} // namespace com::viromania::vtt::wss::KeyboardMatrix
namespace DeviceConnection {
void begin() { delay(connectDelay); }
void poll() { delay(pollDelay); }
} // namespace DeviceConnection
namespace RBootTestHooks {
void maybeFailBeforeServerAcknowledgement(bool, uint32_t) {}
} // namespace RBootTestHooks

int main(int argc, char** argv) {
  assert(argc == 2);
  const std::string scenario = argv[1];
  if (scenario == "wifi-unavailable")
    wifiConnected = false;
  else if (scenario == "provisioning-failure")
    stored = false;
  else if (scenario == "invalid-public-key")
    publicKeyValid = false;
  else if (scenario == "blocked-connect")
    connectDelay = 100000;
  else if (scenario == "blocked-poll")
    pollDelay = 100000;
  else if (scenario == "permanent")
    temporary = false;
  else
    assert(false);
  bool restarted = false;
  try {
    Application::setup();
    while (now < 120000) {
      Application::loop();
      delay(10);
    }
  } catch (const Restart&) {
    restarted = true;
  } catch (const char* message) {
    std::fprintf(stderr, "%s\n", message);
  }
  delete state.serverPublicKey;
  assert(restarted == temporary);
  if (temporary)
    assert(now == 90000);
  else
    assert(!sdkTimer || !sdkTimer->armed);
  std::puts("application deadline regression passed");
}
