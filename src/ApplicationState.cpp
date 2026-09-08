#include "ApplicationState.h"

#include <Arduino.h>

namespace {
ApplicationState state;
}

ApplicationState& applicationState() { return state; }

void printHeapStats() {
  Serial.printf("heap-free=%u; heap-largest=%u; heap-fragmentation=%u%%\n", ESP.getFreeHeap(),
                ESP.getMaxFreeBlockSize(), ESP.getHeapFragmentation());
}
