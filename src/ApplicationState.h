#pragma once

#include <ArduinoWebsockets.h>
#include <ESP8266WiFi.h>
#include <Provisioning.h>
#include <stdint.h>

struct ApplicationState {
  mindflayer::provisioning::Provisioning settings = {};
  websockets::WebsocketsClient client;
  BearSSL::PublicKey* serverPublicKey = nullptr;
  bool provisioned = false;
  bool authenticated = false;
  bool wifiHealthy = false;
  bool wssHealthy = false;
  bool registered = false;
  bool temporaryBoot = false;
  uint32_t temporaryStarted = 0;
};

ApplicationState& applicationState();
void printHeapStats();
