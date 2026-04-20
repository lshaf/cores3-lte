#include "ui.h"
#include "lte.h"

void beep(uint16_t freq, uint32_t ms) {
    M5.Speaker.tone(freq, ms);
}

static void drawSignalBars(int bars) {
    const int barW = 5, gap = 3, baseY = STATUS_H - 3;
    for (int i = 0; i < 4; i++) {
        int bh = 4 + i * 3;
        int bx = DISP_W - 4 - (4 - i) * (barW + gap);
        M5.Display.fillRect(bx, baseY - bh, barW, bh,
                            (i < bars) ? TFT_GREEN : (uint32_t)0x4208);
    }
}

void drawStatusBar() {
    M5.Display.fillRect(0, 0, DISP_W, STATUS_H, 0x2104);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextColor(TFT_WHITE, 0x2104);
    M5.Display.drawString(opName, 5, STATUS_H / 2);
    drawSignalBars(sigBars);
}
