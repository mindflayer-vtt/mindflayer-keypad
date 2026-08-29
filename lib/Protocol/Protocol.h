#pragma once
#include <stddef.h>
#include <stdint.h>
namespace mindflayer { namespace protocol {
constexpr size_t MAX_DEVICE_FRAME_SIZE = 512, MAX_DEVICE_ID = 64, MAX_VERSION = 47, MAX_HARDWARE_ID = 64, MAX_UPDATE_PATH = 191;
enum MessageType : uint8_t { AUTH_CHALLENGE = 0, AUTH_RESPONSE = 1, AUTH_RESULT = 2, REGISTRATION = 3, KEY_EVENT = 4, CONFIGURATION = 5, UPDATE_AVAILABLE = 6 };
enum AuthStatus : uint8_t { AUTH_OK = 0, AUTH_FAILED = 1 };
enum Action : uint8_t { ACTION_UP = 0, ACTION_DOWN = 1 };
struct LedColor { uint8_t r, g, b; };
struct Configuration { LedColor led1, led2; };
struct AuthChallenge { uint8_t challenge[32]; };
struct AuthResult { AuthStatus status; char deviceId[MAX_DEVICE_ID + 1]; };
struct UpdateAvailable { char version[MAX_VERSION + 1]; char path[MAX_UPDATE_PATH + 1]; uint8_t token[32]; uint8_t sha256[32]; uint32_t size; };
bool parseAuthChallenge(const uint8_t*, size_t, AuthChallenge&);
bool buildAuthResponse(uint8_t*, size_t, size_t&, const char*, const uint8_t[32], const uint8_t[32]);
bool parseAuthResult(const uint8_t*, size_t, AuthResult&);
bool buildRegistration(uint8_t*, size_t, size_t&, const char*, const char*);
bool buildKeyEvent(uint8_t*, size_t, size_t&, const char*, bool);
bool parseConfiguration(const uint8_t*, size_t, Configuration&);
bool parseUpdateAvailable(const uint8_t*, size_t, UpdateAvailable&);
bool shouldRestart(bool, bool, bool);
} }
