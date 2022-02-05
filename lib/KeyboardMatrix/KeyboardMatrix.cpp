#include "config.h"
#include "KeyboardMatrix.h"
#include <Arduino.h>

using namespace com::viromania::vtt::wss;

const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
byte rowPins[ROWS] = {5, 4, 0, 2}; //connect to the row pinouts of the kpd
byte colPins[COLS] = {14, 12, 13}; //connect to the column pinouts of the kpd
KeyboardMatrix::KeyState keys[ROWS][COLS] = {
  {{"Q"},{"W"},{"E"}},
  {{"A"},{"S"},{"D"}},
  {{"Z"},{"X"},{"C"}},
  {{"SHI"},{""},{"SPC"}},
};
unsigned long startTime = 0;
const unsigned long waitTime = 5;

KeyboardMatrix::KeyState::KeyState(const char *setKey) {
  strncpy(this->key, setKey, 3);
  this->key[3] = 0;
  this->isDown = false;
}

void KeyboardMatrix::initMatrix() {
  for(int i = 0; i < COLS; i++) {
    pinMode(colPins[i],INPUT_PULLUP);
  }
  for(int i = 0; i < ROWS; i++) {
    pinMode(rowPins[i],OUTPUT);
    digitalWrite(rowPins[i],HIGH);
  }
  startTime = millis();
}

void KeyboardMatrix::detectKeys(void (*callback)(KeyState *key)) {
  if((millis()-startTime) < waitTime) {
    return;
  }
  for(int x = 0; x < ROWS; x++) {
    digitalWrite(rowPins[x],LOW);
    for(int y = 0; y < COLS; y++) {
      if(keys[x][y].isDown == digitalRead(colPins[y])) {
        keys[x][y].isDown = !keys[x][y].isDown;
        callback(&keys[x][y]);
      }
    }
    digitalWrite(rowPins[x],HIGH);
  }
  startTime = millis();
}

KeyboardMatrix::KeyState (*KeyboardMatrix::getState())[4][3] {
  return &keys;
}