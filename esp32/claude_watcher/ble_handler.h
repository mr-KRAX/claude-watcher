// esp32/claude_watcher/ble_handler.h
#pragma once
#include <Arduino.h>
#include "serial_handler.h"   // provides State enum, parseMessage(), isNotification()

// Callback fired on each valid BLE state write (WORKING / WAITING / WAITING_URGENT / IDLE).
// state:    parsed state
// toolName: tool name (non-empty only for WORKING)
typedef void (*BLEStateCallback)(State state, const char* toolName);

// Callback fired when a NOTIFICATION message is received.
typedef void (*BLENotifCallback)();

// Initialize NimBLE peripheral, start advertising.
// stateCallback is invoked on each valid state write.
// notifCallback is invoked on each NOTIFICATION write (may be nullptr).
void bleInit(BLEStateCallback stateCallback, BLENotifCallback notifCallback);
