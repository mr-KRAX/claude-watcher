#include <TFT_eSPI.h>
#include "ble_handler.h"
#include "display.h"

#define SERIAL_BAUD     115200
#define IDLE_TIMEOUT_MS (5UL * 60UL * 1000UL)

// ── Button (IO14, active-low) ──────────────────────────────────────────────
#define BTN_PIN          14
#define DOUBLE_CLICK_MS  400
#define DEBOUNCE_MS       30

TFT_eSPI tft = TFT_eSPI();

State    currentState    = State::IDLE;
char     toolName[32]    = "";
uint32_t lastMsgMs       = 0;
uint8_t  currentRotation = 0;

// Button state
static bool     btnLast     = HIGH;
static uint8_t  clickCount  = 0;
static uint32_t lastClickMs = 0;

// BLE callback — runs on NimBLE FreeRTOS task
static void onBLEState(State state, const char* tool) {
    currentState = state;
    strncpy(toolName, tool, sizeof(toolName) - 1);
    toolName[sizeof(toolName) - 1] = '\0';
    lastMsgMs = millis();
}

static bool pollButton(uint32_t now) {
    bool btnNow = digitalRead(BTN_PIN);
    if (btnLast == HIGH && btnNow == LOW) {
        if (now - lastClickMs > DEBOUNCE_MS) {
            clickCount++;
            lastClickMs = now;
        }
    }
    btnLast = btnNow;
    if (clickCount > 0 && (now - lastClickMs) > DOUBLE_CLICK_MS) {
        uint8_t n = clickCount;
        clickCount = 0;
        return n >= 2;
    }
    return false;
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    pinMode(BTN_PIN, INPUT_PULLUP);
    displayInit(tft);
    bleInit(onBLEState);
    lastMsgMs = millis();
}

void loop() {
    uint32_t now = millis();

    if (pollButton(now)) {
        currentRotation = (currentRotation + 1) % 4;
        displaySetRotation(tft, currentRotation, currentState, toolName);
    }

    if (currentState != State::IDLE && (now - lastMsgMs >= IDLE_TIMEOUT_MS)) {
        currentState = State::IDLE;
        toolName[0] = '\0';
    }

    displayUpdate(tft, currentState, toolName, now);
}
