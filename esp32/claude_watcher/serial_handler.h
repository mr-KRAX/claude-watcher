#pragma once
#include <Arduino.h>

enum class State {
  WORKING,
  WAITING,
  WAITING_URGENT,
  IDLE
};

// Parse a newline-terminated message into a State.
// Returns true if the message was recognized, false otherwise.
// On true, updates *state and (for WORKING) copies tool name into toolName buffer.
bool parseMessage(const String& msg, State* state, char* toolName, size_t toolNameLen);

// Returns true if msg is exactly "NOTIFICATION".
bool isNotification(const String& msg);
