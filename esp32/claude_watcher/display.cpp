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
static bool       lastNotifFrame = false;

// ── Layout helpers ─────────────────────────────────────────────────────────
// All positions derived at runtime so every rotation works.

// Vertical split: crab occupies 65% of screen height
static inline int crabAreaH(TFT_eSPI& tft) { return tft.height() * 65 / 100; }
static inline int statusY(TFT_eSPI& tft)   { return crabAreaH(tft); }
static inline int statusH(TFT_eSPI& tft)   { return tft.height() - crabAreaH(tft); }
static inline int spriteX(TFT_eSPI& tft)   { return (tft.width()  - SPRITE_W)    / 2; }
static inline int spriteY(TFT_eSPI& tft)   { return (crabAreaH(tft) - SPRITE_H_PX) / 2; }

// ── Map a sprite character to a RGB565 color ───────────────────────────────
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

// ── Draw one sprite frame ──────────────────────────────────────────────────
static void drawSprite(TFT_eSPI& tft, const char** spriteRows) {
  int sx = spriteX(tft);
  int sy = spriteY(tft);
  for (int row = 0; row < SPRITE_ROWS; row++) {
    for (int col = 0; col < SPRITE_COLS; col++) {
      uint16_t color = charToColor(spriteRows[row][col]);
      tft.fillRect(
        sx + col * CELL_SIZE,
        sy + row * CELL_SIZE,
        CELL_SIZE, CELL_SIZE,
        color
      );
    }
  }
}

// ── Draw/erase the "!" above the crab for WAITING state ───────────────────
static void drawBang(TFT_eSPI& tft, bool visible) {
  int x = tft.width() / 2;
  int y = spriteY(tft) - 28;
  tft.setTextColor(visible ? COLOR_ALERT : COLOR_BG, COLOR_BG);
  tft.setTextFont(4);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("!", x, y);
  tft.setTextSize(1);
}

// ── Draw/erase floating "z"s for IDLE state ───────────────────────────────
// z's drift upward; offset cycles 0-3.
#define ZZZ_STEP  22   // vertical px between z's (font2×2 = ~32px, step slightly less)
#define ZZZ_COUNT  3

static void drawZzz(TFT_eSPI& tft, uint8_t newOffset, uint8_t prevOffset) {
  int sx = spriteX(tft);
  int sy = spriteY(tft);
  int zx = sx + SPRITE_W + 4;
  int baseY = sy + SPRITE_H_PX - 4;

  // Erase previous positions
  tft.setTextColor(COLOR_BG, COLOR_BG);
  tft.setTextFont(2);
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);
  for (int i = 0; i < ZZZ_COUNT; i++) {
    int y = baseY - (prevOffset * ZZZ_STEP) - (i * ZZZ_STEP);
    if (y >= sy && y < sy + SPRITE_H_PX) {
      tft.drawString("z", zx, y);
    }
  }

  // Draw at new positions
  tft.setTextColor(COLOR_ZZZ, COLOR_BG);
  for (int i = 0; i < ZZZ_COUNT; i++) {
    int y = baseY - (newOffset * ZZZ_STEP) - (i * ZZZ_STEP);
    if (y >= sy && y < sy + SPRITE_H_PX) {
      tft.drawString("z", zx, y);
    }
  }
  tft.setTextSize(1);
}

// ── Draw the status bar ────────────────────────────────────────────────────
static void drawStatusBar(TFT_eSPI& tft, State state, const char* toolName) {
  int sy  = statusY(tft);
  int sh  = statusH(tft);
  int sw  = tft.width();
  int cx  = sw / 2;

  // Clear status area
  tft.fillRect(0, sy, sw, sh, COLOR_BG);
  tft.setTextDatum(TC_DATUM);

  // State label — font 2 × textSize 2 ≈ 32px (bigger than old font 4)
  tft.setTextFont(2);
  tft.setTextSize(2);

  switch (state) {
    case State::WORKING: {
      tft.setTextColor(COLOR_CRAB, COLOR_BG);
      tft.drawString("WORKING", cx, sy + 6);

      // Tool name line — font 4
      tft.setTextSize(1);
      tft.setTextFont(4);
      static const char* SPINNER[] = {"|", "/", "-", "\\"};
      String toolLine = String(SPINNER[frame % 4]) + " " + String(toolName);
      tft.setTextColor(COLOR_GREEN, COLOR_BG);
      tft.drawString(toolLine.c_str(), cx, sy + sh / 2 + 4);
      break;
    }
    case State::WAITING:
    case State::WAITING_URGENT: {
      tft.setTextColor(COLOR_AMBER, COLOR_BG);
      tft.drawString("WAITING", cx, sy + 6);
      tft.setTextSize(1);
      tft.setTextFont(4);
      tft.setTextColor(COLOR_AMBER, COLOR_BG);
      tft.drawString("needs your input", cx, sy + sh / 2 + 4);
      break;
    }
    case State::IDLE: {
      tft.setTextColor(COLOR_ZZZ, COLOR_BG);
      tft.drawString("IDLE", cx, sy + 6);
      tft.setTextSize(1);
      tft.setTextFont(4);
      tft.setTextColor(0x4444, COLOR_BG);
      tft.drawString("sleeping...", cx, sy + sh / 2 + 4);
      break;
    }
  }
  tft.setTextSize(1);
}

#define COLOR_NOTIF  0x001F   // pure blue RGB565

static void drawNotifBorder(TFT_eSPI& tft, bool visible) {
  uint16_t color = visible ? COLOR_NOTIF : COLOR_BG;
  int w = tft.width();
  int h = tft.height();
  int thick = 4;
  tft.fillRect(0,          0,          w,     thick, color);  // top
  tft.fillRect(0,          h - thick,  w,     thick, color);  // bottom
  tft.fillRect(0,          0,          thick, h,     color);  // left
  tft.fillRect(w - thick,  0,          thick, h,     color);  // right
}

// ── Public API ─────────────────────────────────────────────────────────────

void displayInit(TFT_eSPI& tft) {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);
  tft.drawFastHLine(0, statusY(tft) - 1, tft.width(), 0x2104);  // dark grey separator
  drawSprite(tft, (const char**)IDLE_F);
  drawZzz(tft, 0, 0);
  drawStatusBar(tft, State::IDLE, "");
}

void displaySetRotation(TFT_eSPI& tft, uint8_t rotation, State state, const char* toolName) {
  tft.setRotation(rotation);
  tft.fillScreen(COLOR_BG);
  tft.drawFastHLine(0, statusY(tft) - 1, tft.width(), 0x2104);
  frame = 0;
  bangVisible = false;
  zOffset = 0;
  lastNotifFrame = false;
  lastState = state;
  strncpy(lastTool, toolName, sizeof(lastTool) - 1);
  lastTool[sizeof(lastTool) - 1] = '\0';
  // Draw current state immediately
  switch (state) {
    case State::WORKING:
      drawSprite(tft, (const char**)(frame % 2 == 0 ? WORKING_F1 : WORKING_F2));
      break;
    case State::WAITING:
    case State::WAITING_URGENT:
      drawSprite(tft, (const char**)WAITING_F);
      break;
    case State::IDLE:
      drawSprite(tft, (const char**)IDLE_F);
      drawZzz(tft, 0, 0);
      break;
  }
  drawStatusBar(tft, state, toolName);
}

void displayUpdate(TFT_eSPI& tft, State state, const char* toolName, uint32_t now, bool notifActive) {
  uint32_t interval = ANIM_WORKING_MS;
  if (state == State::WAITING || state == State::WAITING_URGENT) interval = ANIM_WAITING_MS;
  if (state == State::IDLE) interval = ANIM_IDLE_MS;

  bool stateChanged = (state != lastState);
  bool frameTick    = (now - lastFrameMs >= interval);

  if (!stateChanged && !frameTick) return;

  if (stateChanged) {
    tft.fillRect(0, 0, tft.width(), crabAreaH(tft), COLOR_BG);
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

  switch (state) {
    case State::WORKING:
      drawSprite(tft, (const char**)(frame % 2 == 0 ? WORKING_F1 : WORKING_F2));
      if (frameTick) drawStatusBar(tft, state, toolName);
      break;

    case State::WAITING:
    case State::WAITING_URGENT:
      drawSprite(tft, (const char**)WAITING_F);
      if (frameTick) {
        bangVisible = !bangVisible;
        drawBang(tft, bangVisible);
      }
      break;

    case State::IDLE:
      drawSprite(tft, (const char**)IDLE_F);
      {
        uint8_t prevOffset = zOffset;
        zOffset = (zOffset + 1) % 4;
        drawZzz(tft, zOffset, prevOffset);
      }
      break;
  }

  // Notification border overlay — independent of crab state
  bool notifFrameOn = notifActive && ((now / 400) % 2);
  if (stateChanged || notifFrameOn != lastNotifFrame) {
    drawNotifBorder(tft, notifFrameOn);
    lastNotifFrame = notifFrameOn;
  }
}
