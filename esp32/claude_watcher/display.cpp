#include "display.h"
#include "crab_sprites.h"
#include <string.h>

// Animation intervals (ms)
#define ANIM_WORKING_MS   300
#define ANIM_WAITING_MS   500
#define ANIM_IDLE_MS     1000

// Internal state
static State      lastState     = State::IDLE;
static uint8_t    frame         = 0;
static uint32_t   lastFrameMs   = 0;
static bool       bangVisible   = false;
static uint8_t    zOffset       = 0;
static char       lastTool[32]  = "";

// Map a sprite character to a RGB565 color
static uint16_t charToColor(char c) {
  switch (c) {
    case 'C': return COLOR_CRAB;
    case 'E': return COLOR_BG;
    case 'A': return COLOR_ALERT;
    case 'D': return COLOR_IDLE;
    case '-': return COLOR_BG;
    default:  return COLOR_BG;
  }
}

// Draw one sprite frame. spriteRows is an array of SPRITE_ROWS char* strings.
static void drawSprite(TFT_eSPI& tft, const char** spriteRows) {
  for (int row = 0; row < SPRITE_ROWS; row++) {
    for (int col = 0; col < SPRITE_COLS; col++) {
      uint16_t color = charToColor(spriteRows[row][col]);
      tft.fillRect(
        SPRITE_X + col * CELL_SIZE,
        SPRITE_Y + row * CELL_SIZE,
        CELL_SIZE, CELL_SIZE,
        color
      );
    }
  }
}

// Draw or erase the "!" above the crab for WAITING state
static void drawBang(TFT_eSPI& tft, bool visible) {
  int x = SCREEN_W / 2;
  int y = SPRITE_Y - 24;
  tft.setTextColor(visible ? COLOR_ALERT : COLOR_BG, COLOR_BG);
  tft.setTextFont(4);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("!", x, y);
}

// Draw or erase floating "z" for IDLE state
static void drawZzz(TFT_eSPI& tft, uint8_t offset) {
  // Erase old z
  tft.setTextColor(COLOR_BG, COLOR_BG);
  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);
  for (int i = 0; i < 3; i++) {
    tft.drawString("z", SPRITE_X + SPRITE_W + 4, SPRITE_Y + 10 + i * 10);
  }
  // Draw new z at offset (drifts upward: offset 0=bottom, wraps at 4)
  tft.setTextColor(COLOR_ZZZ, COLOR_BG);
  for (int i = 0; i < 3; i++) {
    int y = SPRITE_Y + 30 - (offset * 10) - (i * 10);
    if (y >= SPRITE_Y && y < SPRITE_Y + SPRITE_H_PX) {
      tft.drawString("z", SPRITE_X + SPRITE_W + 4, y);
    }
  }
}

// Draw the status bar (state label + tool/message line)
static void drawStatusBar(TFT_eSPI& tft, State state, const char* toolName) {
  // Clear status area
  tft.fillRect(0, STATUS_Y, SCREEN_W, STATUS_H, COLOR_BG);

  tft.setTextDatum(TC_DATUM);

  switch (state) {
    case State::WORKING: {
      // State label
      tft.setTextColor(COLOR_CRAB, COLOR_BG);
      tft.setTextFont(4);
      tft.drawString("WORKING", SCREEN_W / 2, STATUS_Y + 18);
      // Spinner + tool name
      static const char* SPINNER[] = {"|", "/", "-", "\\"};
      String toolLine = String(SPINNER[frame % 4]) + " " + String(toolName);
      tft.setTextColor(COLOR_GREEN, COLOR_BG);
      tft.setTextFont(2);
      tft.drawString(toolLine.c_str(), SCREEN_W / 2, STATUS_Y + 56);
      break;
    }
    case State::WAITING:
    case State::WAITING_URGENT: {
      tft.setTextColor(COLOR_AMBER, COLOR_BG);
      tft.setTextFont(4);
      tft.drawString("WAITING", SCREEN_W / 2, STATUS_Y + 18);
      tft.setTextFont(2);
      tft.drawString("needs your input", SCREEN_W / 2, STATUS_Y + 56);
      break;
    }
    case State::IDLE: {
      tft.setTextColor(COLOR_ZZZ, COLOR_BG);
      tft.setTextFont(4);
      tft.drawString("IDLE", SCREEN_W / 2, STATUS_Y + 18);
      tft.setTextColor(0x4444, COLOR_BG);
      tft.setTextFont(2);
      tft.drawString("sleeping...", SCREEN_W / 2, STATUS_Y + 56);
      break;
    }
  }
}

void displayInit(TFT_eSPI& tft) {
  tft.init();
  tft.setRotation(0);  // portrait
  tft.fillScreen(COLOR_BG);

  // Draw separator line between crab area and status bar
  tft.drawFastHLine(0, STATUS_Y - 1, SCREEN_W, 0x2104);  // dark grey

  drawSprite(tft, (const char**)IDLE_F);
  drawZzz(tft, 0);
  drawStatusBar(tft, State::IDLE, "");
}

void displayUpdate(TFT_eSPI& tft, State state, const char* toolName, uint32_t now) {
  // Determine animation interval for current state
  uint32_t interval = ANIM_WORKING_MS;
  if (state == State::WAITING || state == State::WAITING_URGENT) interval = ANIM_WAITING_MS;
  if (state == State::IDLE) interval = ANIM_IDLE_MS;

  bool stateChanged = (state != lastState);
  bool frameTick    = (now - lastFrameMs >= interval);

  if (!stateChanged && !frameTick) return;  // nothing to do

  if (stateChanged) {
    tft.fillRect(0, 0, SCREEN_W, CRAB_AREA_H, COLOR_BG);  // clear crab area
    frame = 0;
    bangVisible = false;
    zOffset = 0;
    drawStatusBar(tft, state, toolName);
    lastState = state;
    strncpy(lastTool, toolName, sizeof(lastTool) - 1);
    lastTool[sizeof(lastTool) - 1] = '\0';
  }

  if (frameTick) {
    frame++;
    lastFrameMs = now;
  }

  // Draw sprite
  switch (state) {
    case State::WORKING:
      drawSprite(tft, (const char**)(frame % 2 == 0 ? WORKING_F1 : WORKING_F2));
      if (frameTick) drawStatusBar(tft, state, toolName);  // update spinner
      break;

    case State::WAITING:
    case State::WAITING_URGENT:
      drawSprite(tft, (const char**)WAITING_F);
      bangVisible = !bangVisible;
      drawBang(tft, bangVisible);
      break;

    case State::IDLE:
      drawSprite(tft, (const char**)IDLE_F);
      zOffset = (zOffset + 1) % 4;
      drawZzz(tft, zOffset);
      break;
  }
}
