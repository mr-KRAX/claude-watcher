#include "serial_handler.h"
#include <string.h>

bool parseMessage(const String& msg, State* state, char* toolName, size_t toolNameLen) {
  String trimmed = msg;
  trimmed.trim();

  if (trimmed.startsWith("WORKING:")) {
    *state = State::WORKING;
    String tool = trimmed.substring(8);  // after "WORKING:"
    strncpy(toolName, tool.c_str(), toolNameLen - 1);
    toolName[toolNameLen - 1] = '\0';
    return true;
  }

  if (trimmed == "WAITING_URGENT") {
    *state = State::WAITING_URGENT;
    toolName[0] = '\0';
    return true;
  }

  if (trimmed == "WAITING") {
    *state = State::WAITING;
    toolName[0] = '\0';
    return true;
  }

  if (trimmed == "IDLE") {
    *state = State::IDLE;
    toolName[0] = '\0';
    return true;
  }

  return false;  // unknown message — ignored
}
