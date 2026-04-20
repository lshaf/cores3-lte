#include <Arduino.h>
#include <M5Unified.h>
#include "app.h"
#include "lte.h"
#include "ui.h"
#include "screen_menu.h"
#include "screen_ussd.h"
#include "screen_sms.h"

// ── App-wide state (owned here, declared extern in app.h) ────────────────────
volatile Screen currentScreen = SCREEN_MENU;
volatile bool   screenDirty   = false;
volatile bool   menuDirty     = false;
volatile bool   statusDirty   = false;

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    lteMutex = xSemaphoreCreateMutex();
    lte.begin(LTE_BAUD, SERIAL_8N1, LTE_RX, LTE_TX);
    delay(2000);

    M5.Display.drawString("Init LTE...", DISP_W/2, DISP_H/2);

    xSemaphoreTake(lteMutex, portMAX_DELAY);
    if (!lteInit()) {
        xSemaphoreGive(lteMutex);
        M5.Display.drawString("LTE not responding!", DISP_W/2, DISP_H/2);
        return;
    }
    sendAT("ATE0");
    sendAT("AT+CMGF=1"); // SMS text mode

    // Initial one-time status fetch
    String r = sendAT("AT+CSQ");
    int idx = r.indexOf("+CSQ:");
    if (idx >= 0) sigBars = rssiToBars(r.substring(idx+5).toInt());
    r = sendAT("AT+COPS?");
    int q1=r.indexOf('"'), q2=r.indexOf('"',q1+1);
    if (q1>=0 && q2>q1) {
        String op=r.substring(q1+1,q2);
        if (op.length()>14) op=op.substring(0,14);
        strlcpy(opName, op.c_str(), sizeof(opName));
    }
    xSemaphoreGive(lteMutex);

    startSignalTask();

    M5.Display.fillScreen(TFT_BLACK);
    drawStatusBar();
    drawMenuScreen();
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
    M5.update();

    // Screen transition
    if (screenDirty) {
        screenDirty = false;
        switch (currentScreen) {
            case SCREEN_MENU:        drawStatusBar(); drawMenuScreen();       break;
            case SCREEN_USSD:        enterUssdScreen();                       break;
            case SCREEN_SMS_INBOX:   enterSmsInboxScreen();                   break;
            case SCREEN_SMS_COMPOSE: enterSmsComposeScreen();                 break;
            case SCREEN_SMS_READ:    enterSmsReadScreen();                    break;
            default: break;
        }
    }

    // Status bar refresh
    if (statusDirty) { statusDirty=false; drawStatusBar(); }

    // Menu badge refresh
    if (menuDirty && currentScreen==SCREEN_MENU) { menuDirty=false; drawMenuScreen(); }

    // T9 timeout tick
    tickSmsScreen();

    // Touch dispatch
    auto t = M5.Touch.getDetail();
    if (t.wasPressed()) {
        switch (currentScreen) {
            case SCREEN_MENU:        handleMenuTouch(t.x, t.y);  break;
            case SCREEN_USSD:        handleUssdTouch(t.x, t.y);  break;
            case SCREEN_SMS_INBOX:
            case SCREEN_SMS_READ:
            case SCREEN_SMS_COMPOSE: handleSmsTouch(t.x, t.y);   break;
            default: break;
        }
    }

    // URC drain (non-blocking mutex try)
    if (lte.available() && xSemaphoreTake(lteMutex, 0) == pdTRUE) {
        String urc;
        while (lte.available()) urc += (char)lte.read();
        xSemaphoreGive(lteMutex);
        urc.trim();
        if (urc.length()) lte_onURC(urc);
    }
}
