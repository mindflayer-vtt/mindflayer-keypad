#include "ApplicationState.h"

#include "DebugLog.h"

#include <Arduino.h>

namespace {
ApplicationState state;
}

ApplicationState& applicationState() { return state; }

void printHeapStats() {
  DebugLog::printf("heap-free=%u; heap-largest=%u; heap-fragmentation=%u%%\n", ESP.getFreeHeap(),
                   ESP.getMaxFreeBlockSize(), ESP.getHeapFragmentation());
}
