#include "config.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <NeoPixelBus.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <KeyboardMatrix.h>
#include <xtensa/config/core.h>


using namespace websockets;
using namespace com::viromania::vtt::wss;


const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const char* ota_password = OTA_PASSWORD;

const char* websockets_connection_string = WSS_URL;

const char* name = ESP_NAME;

static const int BUFFER_SIZE = 300;
static char buffer[BUFFER_SIZE];

/* NTP Time Servers */
const char *ntp1 = "time.windows.com";
const char *ntp2 = "pool.ntp.org";
time_t now;

/* LEDs*/
#define LED_PIN 3

// Information about the LED strip itself
#define NUM_LEDS 2

#define BRIGHTNESS 128

// The hardcoded certificate authority for this example.
// Don't use it on your own apps!!!!!
// KH, Update Let's Encrypt CA Cert, May 2nd 2021
// Valid from : Sep  4 00:00:00 2020 GMT
// Expired    : Sep 15 16:00:00 2025 GMT
const char ca_cert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// The client's private key which must be kept secret
const char client_private_key[] PROGMEM = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
MIIEowIBAAKCAQEAsRNVTvqP++YUh8NrbXwE83xVsDqcB3F76xcXNKFDERfVd2P/
LvyDovCcoQtT0UCRgPcxRp894EuPH/Ru6Z2Lu85sV//i7ce27tc2WRFSfuhlRxHP
LJWHxTl1CEfXp/owkECQ4MB3pw6Ekc16iTEPiezTG+T+mQ/BkiIwcIK6CMlpR9DI
eYUTqv0f9NrUfAjdBrqlEO2gpgFvLFrkDEU2ntAIc4aPOP7yDOym/xzfy6TiG8Wo
7nlh6M97xTZGfbEPCH9rZDjo5istym1HzF5P+COq+OTSPscjFGXoi978o6hZwa7i
zxorg4h5a5lGnshRu2Gl+Ybfa14OwnIrv/yCswIDAQABAoIBAHxwgbsHCriTcEoY
Yx6F0VTrQ6ydA5mXfuYvS/eIfIE+pp1IgMScYEXZobjrJPQg1CA1l0NyFSHS97oV
JPy34sMQxcLx6KABgeVHCMJ/EeJtnv7a3SUP0GIhhsVS95Lsl8RIG4hWub+EzFVK
eZqAB9N9wr4Pp3wZPodbz37B38rb1QPyMFmQOLlHjKTOmoxsXhL2ot+R3+aLYSur
oPO1kQo7/d0UAZoy8h9OQN4a2EXvawh4O2EvFGbc5X/yXwAdEQ4NPp9VZhkNIRkV
+XZ3FcIqEVOploKtRF/tVBTz3g61/lFz21L9PMmV5y8tvSafr2SpJugGVmp2rrVQ
VNyGlIECgYEA10JSI5gmeCU3zK6kvOfBp54hY/5dDrSUpjKkMxpmm7WZQ6Il/k7A
hMcLeMzHiriT7WhRIXF8AOr2MoEkHkH3DhVNN4ccieVZx2SE5P5mVkItZGLrrpfU
dysR/ARAI1HYegGUiKacZtf9SrRavU0m7fOVOiYwbFRhjyX+MyuteYkCgYEA0pbz
4ZosetScP68uZx1sGlTfkcqLl7i15DHk3gnj6jKlfhvC2MjeLMhNDtKeUAuY7rLQ
guZ0CCghWAv0Glh5eYdfIiPhgqFfX4P5F3Om4zQHVPYj8xHfHG4ZP7dKQTndrO1Q
fLdGDTQLVXabAUSp2YGrijC8J9idSW1pYClvF1sCgYEAjkDn41nzYkbGP1/Swnwu
AEWCL4Czoro32jVxScxSrugt5wJLNWp508VukWBTJhugtq3Pn9hNaJXeKbYqVkyl
pgrxwpZph7+nuxt0r5hnrO2C7eppcjIoWLB/7BorAKxf8REGReBFT7nBTBMwPBW2
el4U6h6+tXh2GJG1Eb/1nnECgYAydVb0THOx7rWNkNUGggc/++why61M6kYy6j2T
cj05BW+f2tkCBoctpcTI83BZb53yO8g4RS2yMqNirGKN2XspwmTqEjzbhv0KLt4F
X4GyWOoU0nFksXiLIFpOaQWSwWG7KJWrfGJ9kWXR0Xxsfl5QLoDCuNCsn3t4d43T
K7phlwKBgHDzF+50+/Wez3YHCy2a/HgSbHCpLQjkknvgwkOh1z7YitYBUm72HP8Z
Ge6b4wEfNuBdlZll/y9BQQOZJLFvJTE5t51X9klrkGrOb+Ftwr7eI/H5xgcadI52
tPYglR5fjuRF/wnt3oX9JlQ2RtSbs+3naXH8JoherHaqNn8UpH0t
-----END RSA PRIVATE KEY-----
)EOF";

// The clint's public certificate which must be shared
const char client_cert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDTzCCAjcCCQDPXvMRYOpeuDANBgkqhkiG9w0BAQsFADCBpjESMBAGA1UEAwwJ
MTI3LjAuMC4xMQswCQYDVQQGEwJVUzElMCMGA1UECgwcTXkgT3duIENlcnRpZmlj
YXRlIEF1dGhvcml0eTEUMBIGA1UECAwLQXJkdWlub0xhbmQxFTATBgNVBAcMDEFy
ZHVpbm9WaWxsZTEVMBMGA1UECgwMRVNQODI2NlVzZXJzMRgwFgYDVQQLDA9FU1A4
MjY2LUFyZHVpbm8wHhcNMTgwMzE0MDQwMDAwWhcNMjkwMjI0MDQwMDAwWjAsMRYw
FAYDVQQKDA1NeSBTZXJ2ZXIgT3JnMRIwEAYDVQQDDAkxMjcuMC4wLjMwggEiMA0G
CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCxE1VO+o/75hSHw2ttfATzfFWwOpwH
cXvrFxc0oUMRF9V3Y/8u/IOi8JyhC1PRQJGA9zFGnz3gS48f9G7pnYu7zmxX/+Lt
x7bu1zZZEVJ+6GVHEc8slYfFOXUIR9en+jCQQJDgwHenDoSRzXqJMQ+J7NMb5P6Z
D8GSIjBwgroIyWlH0Mh5hROq/R/02tR8CN0GuqUQ7aCmAW8sWuQMRTae0Ahzho84
/vIM7Kb/HN/LpOIbxajueWHoz3vFNkZ9sQ8If2tkOOjmKy3KbUfMXk/4I6r45NI+
xyMUZeiL3vyjqFnBruLPGiuDiHlrmUaeyFG7YaX5ht9rXg7Cciu//IKzAgMBAAEw
DQYJKoZIhvcNAQELBQADggEBAEnG+FNyNCOkBvzHiUpHHpScxZqM2f+XDcewJgeS
L6HkYEDIZZDNnd5gduSvkHpdJtWgsvJ7dJZL40w7Ba5sxpZHPIgKJGl9hzMkG+aA
z5GMkjys9h2xpQZx9KL3q7G6A+C0bll7ODZlwBtY07CFMykT4Mp2oMRrQKRucMSV
AB1mKujLAnMRKJ3NM89RQJH4GYiRps9y/HvM5lh7EIK/J0/nEZeJxY5hJngskPKb
oPPdmkR97kaQnll4KNsC3owVlHVU2fMftgYkgQLzyeWgzcNa39AF3B6JlcOzNyQY
seoK24dHmt6tWmn/sbxX7Aa6TL/4mVlFoOgcaTJyVaY/BrY=
-----END CERTIFICATE-----
)EOF";

WebsocketsClient client;
boolean reconnect = false;
time_t lastPong;
DynamicJsonDocument jsonDoc(1024);

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> ledStrip(NUM_LEDS);
RgbColor leds[NUM_LEDS];

void waitForWiFi() {
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  Serial.print("Successfully connected to WiFi: ");
  Serial.print(WiFi.localIP());
  Serial.println();
}

void configureTime() {
  configTime(2 * 3600, 1, ntp1, ntp2);
  while(now < 2 * 3600) {
      Serial.print(".");
      delay(500);
      now = time(nullptr);
  }
  Serial.println("");
  Serial.println("Time set...");
}

void setColors(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
  leds[0].R = r1;
  leds[0].G = g1;
  leds[0].B = b1;
  ledStrip.SetPixelColor(0, leds[0]);
  leds[1].R = r2;
  leds[1].G = g2;
  leds[1].B = b2;
  ledStrip.SetPixelColor(1, leds[1]);
  ledStrip.Show();
}

void onMessageCallback(WebsocketsMessage message) {
  lastPong = time(nullptr);
  Serial.print("Got Message: ");
  Serial.println(message.data());
  auto error = deserializeJson(jsonDoc, message.data());
  if(!error && strcmp(jsonDoc["type"].as<char*>(), "configuration") == 0) {
    setColors(
      jsonDoc["led1"]["r"].as<uint8_t>(),
      jsonDoc["led1"]["g"].as<uint8_t>(),
      jsonDoc["led1"]["b"].as<uint8_t>(),
      jsonDoc["led2"]["r"].as<uint8_t>(),
      jsonDoc["led2"]["g"].as<uint8_t>(),
      jsonDoc["led2"]["b"].as<uint8_t>()
    );
  }
}

void onEventsCallback(WebsocketsEvent event, String data)
{
  if (event == WebsocketsEvent::ConnectionOpened)
  {
    Serial.println("Connection Opened: registering");
    snprintf(buffer, BUFFER_SIZE, "{\"type\":\"registration\",\"controller-id\": \"%s\",\"status\":\"connected\",\"receiver\":false}", name);
    client.send(buffer);
    setColors(0, 255, 0, 0, 0, 0);
  }
  else if (event == WebsocketsEvent::ConnectionClosed)
  {
    Serial.println("Connection Closed: reconnecting");
    setColors(255, 255, 0, 0, 0, 0);
    reconnect = true;
  }
  else if (event == WebsocketsEvent::GotPing)
  {
    Serial.println("Got a Ping! sending Pong");
    client.pong();
  }
  else if (event == WebsocketsEvent::GotPong)
  {
    lastPong = time(nullptr);
  }
}

void setupWebSocket() {
  // run callback when messages are received
  client.onMessage(onMessageCallback);
  
  // run callback when events are occuring
  client.onEvent(onEventsCallback);

  // Before connecting, set the ssl certificates and key of the server
  X509List cert(ca_cert);
  client.setTrustAnchors(&cert);

  X509List *serverCertList = new X509List(client_cert);
  PrivateKey *serverPrivKey = new PrivateKey(client_private_key);
  client.setClientRSACert(serverCertList, serverPrivKey);

  // Connect to server
  client.connect(websockets_connection_string);

  // Send a ping
  client.ping();

  Serial.println("WebSocket connected");
}

void setupLEDs() {
  ledStrip.Begin();
  setColors(255, 0, 0, 0, 0, 0);

  Serial.println("LEDs activated");
}

void setupOTA() {
  ArduinoOTA.begin(WiFi.localIP(), name, ota_password, InternalStorage);
  MDNS.addService("ota", "tcp", 8266);
  Serial.println("OTA is setup ");
  Serial.println(WiFi.hostname());
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);

  setupLEDs();

  WiFi.hostname(name);

  // Connect to wifi
  WiFi.begin(ssid, password);

  Serial.println("Connecting to WiFi...");
  setColors(255, 255, 0, 0, 0, 0);

  // Wait until we are connected to WiFi
  waitForWiFi();

  setupOTA();

  // We configure ESP8266's time, as we need it to validate the certificates
  configureTime();

  KeyboardMatrix::initMatrix();

  setupWebSocket();

  Serial.println("booted!");
  Serial.println();
}

void onKeyChange(KeyboardMatrix::KeyState *key) {
  const char* state = NULL;
  if(key->isDown) {
    state = "down";
  } else {
    state = "up";
  }
  snprintf(buffer, BUFFER_SIZE, "{\"type\":\"key-event\",\"controller-id\": \"%s\",\"key\":\"%s\",\"state\":\"%s\"}", name, key->key, state);
  client.send(buffer);
  Serial.println(key->key);
}

void handleRestartRequest() {
  auto state = KeyboardMatrix::getState();
  if((*state)[0][0].isDown && (*state)[3][0].isDown && (*state)[3][2].isDown) {
    Serial.println("Restarting... ");
    client.close(websockets::CloseReason::CloseReason_GoingAway);
    ESP.restart();
  }
}

void loop() {
  ArduinoOTA.poll();
  client.poll();
  if (client.available()) {
    KeyboardMatrix::detectKeys(onKeyChange);
    handleRestartRequest();
  }
  else if (!client.available() && reconnect)
  {
    reconnect = false;
    client = WebsocketsClient();
    setupWebSocket();
  }
  time_t lastPongDiff = (time(nullptr) - lastPong);
  if (lastPongDiff > 15)
  {
    client.ping();
    if (lastPongDiff > 20)
    {
      client.end();
    }
  }
}