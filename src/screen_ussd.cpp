#include "screen_ussd.h"
#include "app.h"
#include "ui.h"
#include "lte.h"

// ── Screen-local state ────────────────────────────────────────────────────────
enum UssdState { USSD_DIAL, USSD_ACTIVE, USSD_FINAL };
static UssdState ussdState  = USSD_DIAL;
static String    dialInput;
static String    replyInput;
static String    ussdText;

// ── Header ────────────────────────────────────────────────────────────────────
static void drawHeader() {
    M5.Display.fillRect(0, STATUS_H, DISP_W, 22, 0x1082);
    M5.Display.fillRoundRect(2, STATUS_H + 2, 56, 18, 4, 0x4208);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_WHITE, 0x4208);
    M5.Display.setTextSize(1);
    M5.Display.drawString("< MENU", 30, STATUS_H + 11);
    M5.Display.setTextColor(TFT_CYAN, 0x1082);
    M5.Display.drawString("USSD Dialer", DISP_W / 2, STATUS_H + 11);
}

// ── Dial input ────────────────────────────────────────────────────────────────
static void drawDialInput() {
    M5.Display.fillRect(0, INPUT_Y, DISP_W, INPUT_H, TFT_BLACK);
    M5.Display.drawRect(0, INPUT_Y, DISP_W, INPUT_H, 0x4208);
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.drawString(dialInput.length() ? dialInput : " ", DISP_W / 2, INPUT_Y + INPUT_H / 2);
    M5.Display.setTextSize(1);
}

// ── Keypad ────────────────────────────────────────────────────────────────────
static const char DIAL_KEYS[KEY_ROWS][KEY_COLS] = {
    {'1','2','3'}, {'4','5','6'}, {'7','8','9'}, {'*','0','#'}, {0,0,0}
};

static void drawDialKeypad() {
    for (int r = 0; r < KEY_ROWS - 1; r++) {
        for (int c = 0; c < KEY_COLS; c++) {
            int kx = c * KEY_W + 1, ky = PAD_Y + r * KEY_H + 1;
            M5.Display.fillRoundRect(kx, ky, KEY_W - 2, KEY_H - 2, 5, 0x2945);
            char buf[2] = {DIAL_KEYS[r][c], '\0'};
            M5.Display.setTextDatum(middle_center);
            M5.Display.setTextColor(TFT_WHITE, 0x2945);
            M5.Display.setTextSize(2);
            M5.Display.drawString(buf, kx + (KEY_W - 2) / 2, ky + (KEY_H - 2) / 2);
        }
    }
    // Bottom row: SEND | DEL
    int ky = PAD_Y + (KEY_ROWS - 1) * KEY_H + 1, kh = KEY_H - 2;
    M5.Display.fillRoundRect(1,            ky, DISP_W/2 - 2, kh, 5, 0x07E0);
    M5.Display.fillRoundRect(DISP_W/2 + 1, ky, DISP_W/2 - 2, kh, 5, TFT_RED);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_BLACK, 0x07E0);
    M5.Display.drawString("SEND", DISP_W / 4, ky + kh / 2);
    M5.Display.setTextColor(TFT_WHITE, TFT_RED);
    M5.Display.drawString("DEL", DISP_W * 3 / 4, ky + kh / 2);
    M5.Display.setTextSize(1);
}

// ── USSD response popup ───────────────────────────────────────────────────────
static void drawUssdPopup() {
    M5.Display.fillRect(0, POP_Y, DISP_W, DISP_H - POP_Y, 0x1082);
    M5.Display.drawRect(0, POP_Y, DISP_W, POP_TEXT_H + 2, 0x7BEF);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextColor(TFT_WHITE, 0x1082);

    int y = POP_Y + 3, startIdx = 0;
    while (startIdx < (int)ussdText.length() && y < POP_Y + POP_TEXT_H - 8) {
        int nl = ussdText.indexOf('\n', startIdx);
        String line = (nl < 0) ? ussdText.substring(startIdx) : ussdText.substring(startIdx, nl);
        startIdx = (nl < 0) ? ussdText.length() : nl + 1;
        if (line.length() > 42) line = line.substring(0, 42);
        M5.Display.drawString(line, 3, y);
        y += 10;
    }

    if (ussdState == USSD_ACTIVE) {
        M5.Display.fillRoundRect(296, POP_Y + 2, 22, 16, 3, TFT_RED);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setTextColor(TFT_WHITE, TFT_RED);
        M5.Display.drawString("X", 307, POP_Y + 10);
    }

    M5.Display.fillRect(0, POP_IN_Y, DISP_W, POP_IN_H, TFT_BLACK);
    M5.Display.drawRect(0, POP_IN_Y, DISP_W, POP_IN_H, 0x4208);
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextColor(ussdState == USSD_ACTIVE ? TFT_YELLOW : (uint32_t)0x4208, TFT_BLACK);
    M5.Display.drawString((ussdState == USSD_ACTIVE ? "Reply: " : "Done: ") + replyInput,
                          4, POP_IN_Y + POP_IN_H / 2);

    const char popKeys[3][4] = {{'1','2','3','4'},{'5','6','7','8'},{'9','0',0,0}};
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            int kx = c*POP_KEY_W+1, ky = POP_PAD_Y+r*POP_KEY_H+1;
            int kw = POP_KEY_W-2, kh = POP_KEY_H-2;
            if (r==2 && c==2) {
                uint32_t bg = (ussdState==USSD_ACTIVE) ? TFT_RED : (uint32_t)0x2104;
                M5.Display.fillRoundRect(kx,ky,kw,kh,4,bg);
                M5.Display.setTextDatum(middle_center); M5.Display.setTextColor(TFT_WHITE,bg); M5.Display.setTextSize(1);
                M5.Display.drawString("DEL", kx+kw/2, ky+kh/2);
            } else if (r==2 && c==3) {
                uint32_t bg = (ussdState==USSD_ACTIVE) ? (uint32_t)0x07E0 : (uint32_t)0x2104;
                uint32_t fg = (ussdState==USSD_ACTIVE) ? TFT_BLACK : (uint32_t)0x4208;
                M5.Display.fillRoundRect(kx,ky,kw,kh,4,bg);
                M5.Display.setTextDatum(middle_center); M5.Display.setTextColor(fg,bg); M5.Display.setTextSize(1);
                M5.Display.drawString("SEND", kx+kw/2, ky+kh/2);
            } else if (popKeys[r][c]) {
                uint32_t bg = (ussdState==USSD_ACTIVE) ? (uint32_t)0x2945 : (uint32_t)0x1082;
                M5.Display.fillRoundRect(kx,ky,kw,kh,4,bg);
                char buf[2] = {popKeys[r][c],'\0'};
                M5.Display.setTextDatum(middle_center); M5.Display.setTextColor(TFT_WHITE,bg); M5.Display.setTextSize(2);
                M5.Display.drawString(buf, kx+kw/2, ky+kh/2);
            }
        }
    }
    M5.Display.setTextSize(1);
}

// ── USSD send ─────────────────────────────────────────────────────────────────
static void drawCallingOverlay(const String& code, int sec) {
    M5.Display.fillRect(0, STATUS_H, DISP_W, DISP_H-STATUS_H, 0x0841);
    M5.Display.fillRoundRect(20,55,DISP_W-40,130,10,0x1082);
    M5.Display.drawRoundRect(20,55,DISP_W-40,130,10,0x7BEF);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_CYAN,0x1082); M5.Display.setTextSize(2);
    M5.Display.drawString("Dialing...", DISP_W/2, 95);
    M5.Display.setTextColor(TFT_YELLOW,0x1082); M5.Display.setTextSize(1);
    M5.Display.drawString(code, DISP_W/2, 125);
    M5.Display.fillRect(30,153,DISP_W-60,12,0x2104);
    int fill = (DISP_W-60)*sec/60;
    uint32_t col = (sec>20)?TFT_GREEN:(sec>10)?TFT_YELLOW:TFT_RED;
    if (fill>0) M5.Display.fillRect(30,153,fill,12,col);
    M5.Display.setTextColor(TFT_WHITE,0x1082);
    M5.Display.drawString("Timeout in "+String(sec)+"s", DISP_W/2, 175);
    M5.Display.setTextSize(1);
}

static void parseUssdResp(const String& resp) {
    int cidx = resp.indexOf("+CUSD:"); if (cidx < 0) return;
    String after = resp.substring(cidx+6); after.trim();
    int n = after.toInt();
    int q1 = resp.indexOf('"',cidx), q2 = resp.lastIndexOf('"');
    ussdText  = (q1>=0 && q2>q1) ? resp.substring(q1+1,q2) : resp;
    replyInput = "";
    ussdState = (n==1) ? USSD_ACTIVE : USSD_FINAL;
    drawUssdPopup();
}

static void sendUssd(const String& code) {
    Serial.printf("[USSD >>] %s\n", code.c_str());
    xSemaphoreTake(lteMutex, portMAX_DELAY);
    drawCallingOverlay(code, 60);
    lte.println(String("AT+CUSD=1,\"") + code + "\",15");
    String resp; uint32_t start = millis(); int lastSec = 60;
    while (millis()-start < 60000) {
        while (lte.available()) resp += (char)lte.read();
        if (resp.indexOf("+CUSD:") >= 0) break;
        int s = 60-(int)((millis()-start)/1000);
        if (s != lastSec) { lastSec=s; drawCallingOverlay(code,s); }
        delay(50);
    }
    xSemaphoreGive(lteMutex);
    Serial.printf("[LTE <<] %s\n", resp.c_str());
    if (resp.indexOf("+CUSD:") >= 0) {
        parseUssdResp(resp);
    } else {
        M5.Display.fillRoundRect(20,55,DISP_W-40,130,10,0x1082);
        M5.Display.drawRoundRect(20,55,DISP_W-40,130,10,TFT_RED);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setTextColor(TFT_RED,0x1082); M5.Display.setTextSize(2);
        M5.Display.drawString("Timeout", DISP_W/2, 110);
        M5.Display.setTextColor(TFT_WHITE,0x1082); M5.Display.setTextSize(1);
        M5.Display.drawString("No response from network", DISP_W/2, 145);
        delay(2500);
        dialInput = ""; ussdState = USSD_DIAL;
        enterUssdScreen();
    }
}

static void cancelUssd() {
    sendATLocked("AT+CUSD=2");
    ussdState = USSD_DIAL;
    dialInput = "";
    enterUssdScreen();
}

// ── Public API ────────────────────────────────────────────────────────────────
void enterUssdScreen() {
    ussdState  = USSD_DIAL;
    M5.Display.fillScreen(TFT_BLACK);
    drawStatusBar();
    drawHeader();
    drawDialInput();
    drawDialKeypad();
}

void handleUssdTouch(int tx, int ty) {
    if (ussdState != USSD_DIAL) {
        // Popup mode
        if (ty>=POP_Y+2 && ty<=POP_Y+18 && tx>=296 && ussdState==USSD_ACTIVE) {
            beep(600,80); cancelUssd(); return;
        }
        if (ussdState != USSD_ACTIVE) return;
        if (ty < POP_PAD_Y) return;
        int row=(ty-POP_PAD_Y)/POP_KEY_H, col=tx/POP_KEY_W;
        if (row>=3||col>=4) return;
        const char pk[3][4]={{'1','2','3','4'},{'5','6','7','8'},{'9','0',0,0}};
        if (row==2&&col==2) {
            if (replyInput.length()>0) { beep(800,50); replyInput.remove(replyInput.length()-1); drawUssdPopup(); }
        } else if (row==2&&col==3) {
            if (replyInput.length()>0) { beep(1400,80); String r=replyInput; replyInput=""; sendUssd(r); }
        } else if (pk[row][col]) {
            beep(); replyInput+=pk[row][col]; drawUssdPopup();
        }
        return;
    }

    // Dial mode — keypad rows 0-3
    if (ty >= PAD_Y && ty < PAD_Y + (KEY_ROWS-1)*KEY_H) {
        int row=(ty-PAD_Y)/KEY_H, col=tx/KEY_W;
        if (row<KEY_ROWS-1 && col<KEY_COLS) {
            char key=DIAL_KEYS[row][col];
            if (key) { beep(); dialInput+=key; drawDialInput(); }
        }
        return;
    }

    // Bottom row: SEND | DEL
    if (ty >= PAD_Y+(KEY_ROWS-1)*KEY_H) {
        if (tx < DISP_W/2) {
            if (dialInput.length()>0) { beep(1400,80); String code=dialInput; dialInput=""; sendUssd(code); }
        } else {
            if (dialInput.length()>0) { beep(800,50); dialInput.remove(dialInput.length()-1); drawDialInput(); }
        }
    }
}
