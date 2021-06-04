#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <FastLED.h>
#include <ArduinoJson.h>

using namespace websockets;

struct KeyState {
  KeyState(const char *setKey) {
    strncpy(this->key, setKey, 3);
    this->key[3] = 0;
    this->isDown = false;
  }
  char key[4];
  bool isDown;
};

const char* ssid = WIFI_SSID; //Enter SSID
const char* password = WIFI_PASS; //Enter Password

const char* websockets_connection_string = WSS_URL; //Enter server adress

const char* name = CTRL_NAME;// enter the name of the controller

static const byte ROWS = 4; //four rows
static const byte COLS = 3; //three columns
static byte rowPins[ROWS] = {5, 4, 0, 2}; //connect to the row pinouts of the kpd
static byte colPins[COLS] = {14, 12, 13}; //connect to the column pinouts of the kpd
static KeyState keys[ROWS][COLS] = {
  {{"Q"},{"W"},{"E"}},
  {{"A"},{"S"},{"D"}},
  {{"Z"},{"X"},{"C"}},
  {{"SHI"},{""},{"SPC"}},
};
static unsigned long startTime;


static const int BUFFER_SIZE = 300;
static char buffer[BUFFER_SIZE];

/* NTP Time Servers */
const char *ntp1 = "time.windows.com";
const char *ntp2 = "pool.ntp.org";
time_t now;

/* LEDs*/
#define LED_PIN     3

// Information about the LED strip itself
#define NUM_LEDS    2
#define CHIPSET     WS2811
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

#define BRIGHTNESS  128

// The hardcoded certificate authority for this example.
// Don't use it on your own apps!!!!!
// KH, Update Let's Encrypt CA Cert, May 2nd 2021
// Valid from : Sep  4 00:00:00 2020 GMT
// Expired    : Sep 15 16:00:00 2025 GMT
const char ca_cert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIEZTCCA02gAwIBAgIQQAF1BIMUpMghjISpDBbN3zANBgkqhkiG9w0BAQsFADA/
MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT
DkRTVCBSb290IENBIFgzMB4XDTIwMTAwNzE5MjE0MFoXDTIxMDkyOTE5MjE0MFow
MjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxCzAJBgNVBAMT
AlIzMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuwIVKMz2oJTTDxLs
jVWSw/iC8ZmmekKIp10mqrUrucVMsa+Oa/l1yKPXD0eUFFU1V4yeqKI5GfWCPEKp
Tm71O8Mu243AsFzzWTjn7c9p8FoLG77AlCQlh/o3cbMT5xys4Zvv2+Q7RVJFlqnB
U840yFLuta7tj95gcOKlVKu2bQ6XpUA0ayvTvGbrZjR8+muLj1cpmfgwF126cm/7
gcWt0oZYPRfH5wm78Sv3htzB2nFd1EbjzK0lwYi8YGd1ZrPxGPeiXOZT/zqItkel
/xMY6pgJdz+dU/nPAeX1pnAXFK9jpP+Zs5Od3FOnBv5IhR2haa4ldbsTzFID9e1R
oYvbFQIDAQABo4IBaDCCAWQwEgYDVR0TAQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8E
BAMCAYYwSwYIKwYBBQUHAQEEPzA9MDsGCCsGAQUFBzAChi9odHRwOi8vYXBwcy5p
ZGVudHJ1c3QuY29tL3Jvb3RzL2RzdHJvb3RjYXgzLnA3YzAfBgNVHSMEGDAWgBTE
p7Gkeyxx+tvhS5B1/8QVYIWJEDBUBgNVHSAETTBLMAgGBmeBDAECATA/BgsrBgEE
AYLfEwEBATAwMC4GCCsGAQUFBwIBFiJodHRwOi8vY3BzLnJvb3QteDEubGV0c2Vu
Y3J5cHQub3JnMDwGA1UdHwQ1MDMwMaAvoC2GK2h0dHA6Ly9jcmwuaWRlbnRydXN0
LmNvbS9EU1RST09UQ0FYM0NSTC5jcmwwHQYDVR0OBBYEFBQusxe3WFbLrlAJQOYf
r52LFMLGMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjANBgkqhkiG9w0B
AQsFAAOCAQEA2UzgyfWEiDcx27sT4rP8i2tiEmxYt0l+PAK3qB8oYevO4C5z70kH
ejWEHx2taPDY/laBL21/WKZuNTYQHHPD5b1tXgHXbnL7KqC401dk5VvCadTQsvd8
S8MXjohyc9z9/G2948kLjmE6Flh9dDYrVYA9x2O+hEPGOaEOa1eePynBgPayvUfL
qjBstzLhWVQLGAkXXmNs+5ZnPBxzDJOLxhF2JIbeQAcH5H0tZrUlo5ZYyOqA7s9p
O5b85o3AM/OJ+CktFBQtfvBhcJVd9wvlwPsk+uyOy2HI7mNxKKgsBTt375teA2Tw
UdHkhVNcsAKX1H7GNNLOEADksd86wuoXvg==
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
time_t lastPong;
DynamicJsonBuffer jsonBuffer(300);

void onMessageCallback(WebsocketsMessage message) {
  // Serial.print("Got Message: ");
  // Serial.println(message.data());
  // JsonObject& root = jsonBuffer.parseObject(message.data());
  // if(strcmp(root["type"].as<char*>(), "configuration") == 0) {
  //   leds[0].r = root["led1"]["r"].as<uint8_t>();
  //   leds[0].g = root["led1"]["g"].as<uint8_t>();
  //   leds[0].b = root["led1"]["b"].as<uint8_t>();
  //   leds[1].r = root["led2"]["r"].as<uint8_t>();
  //   leds[1].g = root["led2"]["g"].as<uint8_t>();
  //   leds[1].b = root["led2"]["b"].as<uint8_t>();
  // }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if(event == WebsocketsEvent::ConnectionOpened) {
    Serial.println("Connection Opened: registering");
    snprintf(buffer, BUFFER_SIZE, "{\"type\":\"registration\",\"controller-id\": \"%s\",\"status\":\"connected\",\"receiver\":\"false\"}", name);
    client.send(buffer);
  } else if(event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("Connection Closed: reconnecting");
    client.connect(websockets_connection_string);
  } else if(event == WebsocketsEvent::GotPing) {
    Serial.println("Got a Ping! sending Pong");
    client.pong();
  } else if(event == WebsocketsEvent::GotPong) {
    lastPong = time(nullptr);
  }
}

void setup() {
  Serial.begin(115200);
  // Connect to wifi
  WiFi.begin(ssid, password);

  Serial.println("Connecting to WiFi...");

  for(int i = 0; i < COLS; i++) {
    pinMode(colPins[i],INPUT_PULLUP);
  }
  for(int i = 0; i < ROWS; i++) {
    pinMode(rowPins[i],OUTPUT);
    digitalWrite(rowPins[i],HIGH);
  }


  // Wait until we are connected to WiFi
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("Successfully connected to WiFi, setting time... ");

  // We configure ESP8266's time, as we need it to validate the certificates
  configTime(2 * 3600, 1, ntp1, ntp2);
  while(now < 2 * 3600) {
      Serial.print(".");
      delay(500);
      now = time(nullptr);
  }
  Serial.println("");
  Serial.println("Time set, connecting to server...");

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

  startTime = millis();

  //LED init
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalSMD5050 );
  FastLED.setBrightness( BRIGHTNESS );
}

void loop() {
  const char* state = NULL;
  client.poll();
  if (client.available() && (millis()-startTime)>100) {
    for(int x = 0; x < ROWS; x++) {
      digitalWrite(rowPins[x],LOW);
      for(int y = 0; y < COLS; y++) {
        if(keys[x][y].isDown == digitalRead(colPins[y])) {
          keys[x][y].isDown = !keys[x][y].isDown;
          if(keys[x][y].isDown) {
            state = "down";
          } else {
            state = "up";
          }
          snprintf(buffer, BUFFER_SIZE, "{\"type\":\"key-event\",\"controller-id\": \"%s\",\"key\":\"%s\",\"state\":\"%s\"}", name, keys[x][y].key, state);
          client.send(buffer);

          state = NULL;
        }
      }
      digitalWrite(rowPins[x],HIGH);
    }
    startTime = millis();
  }
  time_t lastPongDiff = (time(nullptr) - lastPong);
  if(lastPongDiff > 15) {
    client.ping();
    if(lastPongDiff > 20) {
      client.end();
    }
  }
}