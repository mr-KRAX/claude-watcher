#pragma once

// Sprite grid: 12 cols × 9 rows, 8px per cell → 96×72px total
#define SPRITE_COLS   12
#define SPRITE_ROWS    9
#define CELL_SIZE      8

// Precomputed RGB565 colors
// color565 formula: (r & 0xF8) << 8 | (g & 0xFC) << 3 | b >> 3
#define COLOR_CRAB    0xCBEA  // #CD7F55 — crab orange
#define COLOR_IDLE    0x8AE5  // #8B5E45 — dim crab (sleeping)
#define COLOR_ALERT   0xFC80  // #FF9800 — orange alert eyes
#define COLOR_ZZZ     0x5B58  // #5C6BC0 — blue-grey z's
#define COLOR_GREEN   0x4D6A  // #4CAF50 — working spinner
#define COLOR_AMBER   0xFC80  // same as alert, used for WAITING label
#define COLOR_BG      0x0000  // TFT_BLACK

// ── WORKING FRAME 1: normal stance ────────────────────────────────────────
// Legs at cols 3-4 and 7-8, both pairs level
static const char* WORKING_F1[SPRITE_ROWS] = {
  "............",  // row 0
  "..CCCCCCCC..",  // row 1  body top
  "..CEECEECC..",  // row 2  open eyes: E at 3-4, 6-7
  "..CEECEECC..",  // row 3
  "CCCCCCCCCCCC",  // row 4  full-width claw row
  "..CCCCCCCC..",  // row 5  body bottom
  "...CC..CC...",  // row 6  legs: cols 3-4, 7-8
  "...CC..CC...",  // row 7
  "............",  // row 8
};

// ── WORKING FRAME 2: legs alternate ───────────────────────────────────────
// Bottom leg row spreads outward to simulate walking
static const char* WORKING_F2[SPRITE_ROWS] = {
  "............",
  "..CCCCCCCC..",
  "..CEECEECC..",
  "..CEECEECC..",
  "CCCCCCCCCCCC",
  "..CCCCCCCC..",
  "...CC..CC...",  // row 6 same
  "..CC....CC..",  // row 7 legs spread: cols 2-3 and 8-9
  "............",
};

// ── WAITING: raised claws, alert eyes ─────────────────────────────────────
// '!' is drawn separately above the sprite (blinks on/off)
static const char* WAITING_F[SPRITE_ROWS] = {
  "CC........CC",  // row 0  raised claws at corners
  "..CCCCCCCC..",  // row 1
  "..CAACAACC..",  // row 2  A = orange alert eyes
  "..CAACAACC..",  // row 3
  "..CCCCCCCC..",  // row 4  no extended claws (they're raised)
  "..CCCCCCCC..",  // row 5
  "...CC..CC...",  // row 6
  "...CC..CC...",  // row 7
  "............",  // row 8
};

// ── IDLE: sleeping crab ────────────────────────────────────────────────────
// D = dimmed crab color, '-' = closed eye line (black)
// 'z' is drawn separately above the sprite (drifts upward)
static const char* IDLE_F[SPRITE_ROWS] = {
  "............",
  "..DDDDDDDD..",  // row 1  body top (dimmed)
  "..DDDDDDDD..",  // row 2  no eye holes — solid
  "..D--D--DD..",  // row 3  closed eyes: '-' at 3-4 and 6-7
  "DDDDDDDDDDDD",  // row 4  claws (dimmed)
  "..DDDDDDDD..",  // row 5
  "...DD..DD...",  // row 6  legs (dimmed)
  "...DD..DD...",  // row 7
  "............",  // row 8
};
