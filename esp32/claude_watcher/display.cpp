#include "display.h"
#include "crab_sprites.h"
#include <string.h>
#include <Fonts/GFXFF/FreeMonoBold18pt7b.h>
#include <Fonts/GFXFF/FreeMonoBold12pt7b.h>

// Animation intervals (ms)
#define ANIM_WORKING_MS   300
#define ANIM_WAITING_MS   500
#define ANIM_IDLE_MS      500

// Internal state
static State      lastState     = State::IDLE;
static uint8_t    frame         = 0;
static uint32_t   lastFrameMs   = 0;
static bool       bangVisible   = false;
static uint16_t   zPos          = 0;
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
  uint16_t color = visible ? COLOR_ALERT : COLOR_BG;
  tft.setTextColor(color, COLOR_BG);
  tft.setTextFont(4);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM);

  bool horiz = (tft.getRotation() % 2 == 1);
  if (horiz) {
    int sx   = spriteX(tft);
    int sy   = spriteY(tft);
    int midY = sy + SPRITE_H_PX / 2;
    // Center each ! in the gap between crab and screen edge
    int rightX = sx + SPRITE_W + (tft.width() - sx - SPRITE_W) / 2;
    int leftX  = sx / 2;
    tft.drawString("!", rightX, midY);
    tft.drawString("!", leftX,  midY);
  } else {
    int x = tft.width() / 2;
    int y = spriteY(tft) - 28;
    tft.drawString("!", x, y);
  }
  tft.setTextSize(1);
}

// ── Draw/erase floating "z"s for IDLE state ───────────────────────────────
#define ZZZ_STEP   28   // > z glyph height (11px) — gives clear gap between z's
#define ZZZ_COUNT   3
// ZZZ_CYCLE must be divisible by ZZZ_STEP; 6 steps = full snake + pause before restart.
#define ZZZ_CYCLE  168  // 6 * ZZZ_STEP

static void drawZzz(TFT_eSPI& tft, uint16_t newPos, uint16_t prevPos) {
  int sx      = spriteX(tft);
  int sy      = spriteY(tft);
  int baseY   = sy + SPRITE_H_PX - 4;
  bool horiz  = (tft.getRotation() % 2 == 1);

  // Center each z in its gap (TC_DATUM); drift outward as it rises
  int halfGap  = (tft.width() - sx - SPRITE_W) / 2;  // = left gap = right gap
  int rightCX  = sx + SPRITE_W + halfGap;              // centre of right gap
  int leftCX   = sx - halfGap;                          // centre of left gap
  int maxDrift = max(4, halfGap / 4);                  // outward drift over full travel

  tft.setFreeFont(&FreeMonoBold12pt7b);
  tft.setTextDatum(TC_DATUM);

  // Erase previous positions
  tft.setTextColor(COLOR_BG, COLOR_BG);
  for (int i = 0; i < ZZZ_COUNT; i++) {
    int yOff = (int)prevPos - i * ZZZ_STEP;
    if (yOff < 0) continue;
    int y = baseY - yOff;
    if (y < sy || y >= sy + SPRITE_H_PX) continue;
    int drift = yOff * maxDrift / (SPRITE_H_PX - 4);
    tft.drawString("z", rightCX + drift, y);
    if (horiz) tft.drawString("z", leftCX - drift, y);
  }

  // Draw at new positions
  tft.setTextColor(COLOR_ZZZ, COLOR_BG);
  for (int i = 0; i < ZZZ_COUNT; i++) {
    int yOff = (int)newPos - i * ZZZ_STEP;
    if (yOff < 0) continue;
    int y = baseY - yOff;
    if (y < sy || y >= sy + SPRITE_H_PX) continue;
    int drift = yOff * maxDrift / (SPRITE_H_PX - 4);
    tft.drawString("z", rightCX + drift, y);
    if (horiz) tft.drawString("z", leftCX - drift, y);
  }
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

  // State label — FreeMonoBold18pt (~35px line height)
  tft.setFreeFont(&FreeMonoBold18pt7b);

  switch (state) {
    case State::WORKING: {
      tft.setTextColor(COLOR_CRAB, COLOR_BG);
      tft.drawString("WORKING", cx, sy + 4);

      tft.setFreeFont(&FreeMonoBold12pt7b);
      static const char* SPINNER[] = {"|", "/", "-", "\\"};
      String toolLine = String(SPINNER[frame % 4]) + " " + String(toolName);
      tft.setTextColor(COLOR_GREEN, COLOR_BG);
      tft.drawString(toolLine.c_str(), cx, sy + 40);
      break;
    }
    case State::WAITING:
    case State::WAITING_URGENT: {
      tft.setTextColor(COLOR_AMBER, COLOR_BG);
      tft.drawString("WAITING", cx, sy + 4);
      tft.setFreeFont(&FreeMonoBold12pt7b);
      tft.setTextColor(COLOR_AMBER, COLOR_BG);
      tft.drawString("needs your input", cx, sy + 40);
      break;
    }
    case State::IDLE: {
      tft.setTextColor(COLOR_ZZZ, COLOR_BG);
      tft.drawString("IDLE", cx, sy + 4);
      tft.setFreeFont(&FreeMonoBold12pt7b);
      tft.setTextColor(0x4444, COLOR_BG);
      tft.drawString("sleeping...", cx, sy + 40);
      break;
    }
  }
}

#define COLOR_NOTIF  0x001F   // pure blue RGB565

static void drawNotifBorder(TFT_eSPI& tft, bool visible) {
  uint16_t color = visible ? COLOR_NOTIF : COLOR_BG;
  int w = tft.width();
  int h = tft.height();
  int thick = 4;
  tft.fillRect(0,          0,          w,     thick, color);  // top
  tft.fillRect(0,          h - thick,  w,     thick, color);  // bottom
  int ch = crabAreaH(tft);
  tft.fillRect(0,          0,          thick, ch,    color);  // left (crab area only)
  tft.fillRect(w - thick,  0,          thick, ch,    color);  // right (crab area only)
}

// ── Public API ─────────────────────────────────────────────────────────────

void displayInit(TFT_eSPI& tft) {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);
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
  zPos = 0;
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
    zPos = 0;
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
        uint16_t prevPos = zPos;
        zPos = (zPos + ZZZ_STEP) % ZZZ_CYCLE;
        drawZzz(tft, zPos, prevPos);
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
