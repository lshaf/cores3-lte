#pragma once
#include <M5Unified.h>

// ── Display layout ────────────────────────────────────────────────────────────
constexpr int DISP_W   = 320;
constexpr int DISP_H   = 240;
constexpr int STATUS_H = 22;

// Dial keypad (USSD screen)
constexpr int INPUT_Y  = 46;
constexpr int INPUT_H  = 36;
constexpr int PAD_Y    = 84;
constexpr int KEY_COLS = 3;
constexpr int KEY_ROWS = 5;   // rows 0-3 = digits, row 4 = BACK/SEND/DEL
constexpr int KEY_W    = 106;
constexpr int KEY_H    = 31;  // (240-84)/5

// USSD response popup
constexpr int POP_Y      = 24;
constexpr int POP_TEXT_H = 106;
constexpr int POP_IN_Y   = 132;
constexpr int POP_IN_H   = 20;
constexpr int POP_PAD_Y  = 154;
constexpr int POP_KEY_W  = 80;
constexpr int POP_KEY_H  = 28;

// ── Functions ─────────────────────────────────────────────────────────────────
void beep(uint16_t freq = 1200, uint32_t ms = 60);
void drawStatusBar();
