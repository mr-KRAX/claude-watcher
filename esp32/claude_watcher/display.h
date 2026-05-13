#pragma once
#include <TFT_eSPI.h>
#include "serial_handler.h"
#include "crab_sprites.h"   // provides SPRITE_COLS, SPRITE_ROWS, CELL_SIZE, COLOR_*

// Screen dimensions are dynamic — use tft.width() / tft.height() at runtime.
// All four rotations are supported; layout auto-adapts.

// Sprite pixel dimensions (derived from crab_sprites.h constants)
#define SPRITE_W     (SPRITE_COLS * CELL_SIZE)   // 96
#define SPRITE_H_PX  (SPRITE_ROWS * CELL_SIZE)   // 72

// Initialize display and draw first frame
void displayInit(TFT_eSPI& tft);

// Call from loop(). Advances animation and redraws if needed.
// state:       current state
// toolName:    current tool name (used in WORKING state)
// now:         current millis() value
// notifActive: true if a NOTIFICATION is within its 2-minute window
void displayUpdate(TFT_eSPI& tft, State state, const char* toolName, uint32_t now, bool notifActive);

// Change screen rotation (0-3) and force a full redraw.
void displaySetRotation(TFT_eSPI& tft, uint8_t rotation, State state, const char* toolName);
