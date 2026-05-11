#include <TFT_eSPI.h>
#include "serial_handler.h"
#include "display.h"

#define SERIAL_BAUD     115200
#define IDLE_TIMEOUT_MS (5UL * 60UL * 1000UL)  // 5 minutes

TFT_eSPI tft = TFT_eSPI();

State    currentState   = State::IDLE;
char     toolName[32]   = "";
uint32_t lastMsgMs      = 0;
String   serialBuf      = "";

void setup() {
  Serial.begin(SERIAL_BAUD);
  displayInit(tft);
  lastMsgMs = millis();
}

void loop() {
  uint32_t now = millis();

  // Read serial — accumulate until newline
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      State newState = currentState;
      if (parseMessage(serialBuf, &newState, toolName, sizeof(toolName))) {
        currentState = newState;
        lastMsgMs = now;
      }
      serialBuf = "";
    } else {
      serialBuf += c;
      if (serialBuf.length() > 64) serialBuf = "";  // guard against runaway input
    }
  }

  // Auto-idle after timeout
  if (currentState != State::IDLE && (now - lastMsgMs >= IDLE_TIMEOUT_MS)) {
    currentState = State::IDLE;
    toolName[0] = '\0';
  }

  displayUpdate(tft, currentState, toolName, now);
}
