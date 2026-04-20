#include "screen_menu.h"
#include "app.h"
#include "ui.h"
#include "lte.h"

#define ITEM_X  20
#define ITEM_W  280
#define ITEM_H  56
#define ITEM1_Y 47
#define ITEM2_Y 109
#define ITEM3_Y 171

void drawMenuScreen() {
    M5.Display.fillRect(0, STATUS_H, DISP_W, DISP_H - STATUS_H, TFT_BLACK);
    drawStatusBar();

    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.drawString("LTE COMX", DISP_W / 2, 34);

    // ── USSD button ───────────────────────────────────────────────────────────
    M5.Display.fillRoundRect(ITEM_X, ITEM1_Y, ITEM_W, ITEM_H, 10, 0x2945);
    M5.Display.drawRoundRect(ITEM_X, ITEM1_Y, ITEM_W, ITEM_H, 10, 0x7BEF);
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextColor(TFT_WHITE, 0x2945);
    M5.Display.setTextSize(2);
    M5.Display.drawString("USSD Dialer", ITEM_X + 16, ITEM1_Y + ITEM_H / 2);

    // ── SMS Inbox button ──────────────────────────────────────────────────────
    M5.Display.fillRoundRect(ITEM_X, ITEM2_Y, ITEM_W, ITEM_H, 10, 0x2945);
    M5.Display.drawRoundRect(ITEM_X, ITEM2_Y, ITEM_W, ITEM_H, 10, 0x7BEF);
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextColor(TFT_WHITE, 0x2945);
    M5.Display.setTextSize(2);
    M5.Display.drawString("SMS Inbox", ITEM_X + 16, ITEM2_Y + ITEM_H / 2);

    // Unread badge on SMS Inbox button
    if (unreadSMS > 0) {
        int bx = ITEM_X + ITEM_W - 44, by = ITEM2_Y + (ITEM_H - 28) / 2;
        M5.Display.fillRoundRect(bx, by, 40, 28, 6, TFT_RED);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setTextColor(TFT_WHITE, TFT_RED);
        M5.Display.setTextSize(1);
        M5.Display.drawString(String(unreadSMS), bx + 20, by + 14);
    }

    // ── Compose SMS button ────────────────────────────────────────────────────
    M5.Display.fillRoundRect(ITEM_X, ITEM3_Y, ITEM_W, ITEM_H, 10, 0x1450);
    M5.Display.drawRoundRect(ITEM_X, ITEM3_Y, ITEM_W, ITEM_H, 10, 0x07E0);
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextColor(TFT_WHITE, 0x1450);
    M5.Display.setTextSize(2);
    M5.Display.drawString("Compose SMS", ITEM_X + 16, ITEM3_Y + ITEM_H / 2);

    M5.Display.setTextSize(1);
}

void handleMenuTouch(int tx, int ty) {
    if (tx >= ITEM_X && tx <= ITEM_X + ITEM_W) {
        if (ty >= ITEM1_Y && ty <= ITEM1_Y + ITEM_H) {
            beep(); currentScreen = SCREEN_USSD; screenDirty = true;
        } else if (ty >= ITEM2_Y && ty <= ITEM2_Y + ITEM_H) {
            beep(); currentScreen = SCREEN_SMS_INBOX; screenDirty = true;
        } else if (ty >= ITEM3_Y && ty <= ITEM3_Y + ITEM_H) {
            beep(); currentScreen = SCREEN_SMS_COMPOSE; screenDirty = true;
        }
    }
}
