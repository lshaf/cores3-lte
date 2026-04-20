#include "lte.h"
#include "app.h"

HardwareSerial    lte(1);
SemaphoreHandle_t lteMutex       = nullptr;
volatile int      sigBars        = 0;
char              opName[20]     = "...";
volatile int      unreadSMS      = 0;

// ── helpers ───────────────────────────────────────────────────────────────────

int rssiToBars(int rssi) {
    if (rssi == 99 || rssi < 1) return 0;
    if (rssi < 7)  return 1;
    if (rssi < 12) return 2;
    if (rssi < 18) return 3;
    return 4;
}

// Caller must hold lteMutex
String sendAT(const char* cmd, uint32_t timeout) {
    Serial.printf("[LTE >>] %s\n", cmd);
    lte.println(cmd);
    String resp;
    uint32_t t = millis();
    while (millis() - t < timeout) while (lte.available()) resp += (char)lte.read();
    resp.trim();
    Serial.printf("[LTE <<] %s\n", resp.c_str());
    return resp;
}

String sendATLocked(const char* cmd, uint32_t timeout) {
    xSemaphoreTake(lteMutex, portMAX_DELAY);
    String r = sendAT(cmd, timeout);
    xSemaphoreGive(lteMutex);
    return r;
}

// ── URC handler (called from main loop with mutex briefly held) ───────────────

void lte_onURC(const String& urc) {
    Serial.printf("[LTE URC] %s\n", urc.c_str());
    if (urc.indexOf("+CMTI:") >= 0) {
        unreadSMS++;
        menuDirty  = true;
        statusDirty = true;
    }
}

void lte_clearUnread() {
    unreadSMS  = 0;
    menuDirty  = true;
    statusDirty = true;
}

// ── Background signal task (core 0) ──────────────────────────────────────────

static void signalTask(void*) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        if (xSemaphoreTake(lteMutex, pdMS_TO_TICKS(5000)) != pdTRUE) continue;

        // Signal strength
        lte.println("AT+CSQ");
        String r; uint32_t t = millis();
        while (millis() - t < 3000) { while (lte.available()) r += (char)lte.read(); vTaskDelay(10); }
        int idx = r.indexOf("+CSQ:");
        if (idx >= 0) sigBars = rssiToBars(r.substring(idx + 5).toInt());

        // Operator name
        lte.println("AT+COPS?");
        r = ""; t = millis();
        while (millis() - t < 3000) { while (lte.available()) r += (char)lte.read(); vTaskDelay(10); }
        int q1 = r.indexOf('"'), q2 = r.indexOf('"', q1 + 1);
        if (q1 >= 0 && q2 > q1) {
            String op = r.substring(q1 + 1, q2);
            if (op.length() > 14) op = op.substring(0, 14);
            strlcpy(opName, op.c_str(), sizeof(opName));
        }

        // Unread SMS count
        lte.println("AT+CMGL=\"UNREAD\"");
        r = ""; t = millis();
        while (millis() - t < 4000) { while (lte.available()) r += (char)lte.read(); vTaskDelay(10); }
        int cnt = 0, i = 0;
        while ((i = r.indexOf("+CMGL:", i)) >= 0) { cnt++; i++; }
        unreadSMS = cnt;

        xSemaphoreGive(lteMutex);
        statusDirty = true;
        menuDirty   = true;
    }
}

// ── Init ──────────────────────────────────────────────────────────────────────

bool lteInit() {
    for (int i = 0; i < 5; i++) {
        if (sendAT("AT").indexOf("OK") >= 0) return true;
        delay(1000);
    }
    return false;
}

void startSignalTask() {
    xTaskCreatePinnedToCore(signalTask, "signal", 4096, nullptr, 1, nullptr, 0);
}
