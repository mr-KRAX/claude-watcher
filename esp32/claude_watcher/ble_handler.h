// esp32/claude_watcher/ble_handler.h
#pragma once
#include <Arduino.h>
#include "serial_handler.h"   // provides State enum and parseMessage()

// Callback fired on each valid BLE write.
// state:    parsed state
// toolName: tool name (non-empty only for WORKING)
typedef void (*BLEStateCallback)(State state, const char* toolName);

// Initialize NimBLE peripheral, start advertising.
// callback is invoked from the NimBLE task on each valid characteristic write.
void bleInit(BLEStateCallback callback);
