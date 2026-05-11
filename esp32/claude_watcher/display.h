#pragma once
#include <TFT_eSPI.h>
#include "serial_handler.h"
#include "crab_sprites.h"   // provides SPRITE_COLS, SPRITE_ROWS, CELL_SIZE, COLOR_*

// Screen regions
#define SCREEN_W        170
#define SCREEN_H        320
#define CRAB_AREA_H     210
#define STATUS_Y        210
#define STATUS_H        110

// Sprite layout (depends on SPRITE_COLS/ROWS/CELL_SIZE from crab_sprites.h)
#define SPRITE_W         (SPRITE_COLS * CELL_SIZE)    // 96
#define SPRITE_H_PX      (SPRITE_ROWS * CELL_SIZE)    // 72
#define SPRITE_X         ((SCREEN_W - SPRITE_W) / 2)  // 37
#define SPRITE_Y         ((CRAB_AREA_H - SPRITE_H_PX) / 2) // 69

// Initialize display and draw first frame
void displayInit(TFT_eSPI& tft);

// Call from loop(). Advances animation and redraws if needed.
// state: current state
// toolName: current tool name (used in WORKING state)
// now: current millis() value
void displayUpdate(TFT_eSPI& tft, State state, const char* toolName, uint32_t now);
