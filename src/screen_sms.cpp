#include "screen_sms.h"
#include "app.h"
#include "ui.h"
#include "lte.h"

static void drawReadScreen(); // forward declaration

// ── SMS message struct ────────────────────────────────────────────────────────
struct SmsMsg {
    int  index;
    char sender[24];
    char date[22];
    char body[164];
    bool read;
};

#define MAX_SMS_INBOX 10

static SmsMsg  inbox[MAX_SMS_INBOX];
static int     inboxCount    = 0;
static int     selectedMsg   = 0;  // index into inbox[] for read view

// ── Compose state ─────────────────────────────────────────────────────────────
static String  composeNum;
static String  composeBody;
static int     composeField  = 0;  // 0=number, 1=body

// T9 multi-tap state
static int     t9Key         = -1;
static int     t9Count       = 0;
static uint32_t t9Time       = 0;
#define T9_TIMEOUT 1500

static const char* T9_MAP[] = {
    " 0",     // 0 → space / 0
    ".,!?1",  // 1
    "ABC2",   // 2
    "DEF3",   // 3
    "GHI4",   // 4
    "JKL5",   // 5
    "MNO6",   // 6
    "PQRS7",  // 7
    "TUV8",   // 8
    "WXYZ9",  // 9
};

// ── Helpers ───────────────────────────────────────────────────────────────────
static void drawSmsHeader(const char* title) {
    M5.Display.fillRect(0, STATUS_H, DISP_W, 22, 0x1082);
    M5.Display.fillRoundRect(2, STATUS_H+2, 56, 18, 4, 0x4208);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_WHITE, 0x4208);
    M5.Display.setTextSize(1);
    M5.Display.drawString("< MENU", 30, STATUS_H+11);
    M5.Display.setTextColor(TFT_CYAN, 0x1082);
    M5.Display.drawString(title, DISP_W/2, STATUS_H+11);
}

// ── Inbox loading ─────────────────────────────────────────────────────────────
static void loadInbox() {
    inboxCount = 0;
    xSemaphoreTake(lteMutex, portMAX_DELAY);
    sendAT("AT+CMGF=1"); // text mode
    String raw = sendAT("AT+CMGL=\"ALL\"", 8000);
    xSemaphoreGive(lteMutex);

    // Parse +CMGL: <idx>,"<stat>","<from>","<alpha>","<date>"
    // next line = body
    int pos = 0;
    while (pos < (int)raw.length() && inboxCount < MAX_SMS_INBOX) {
        int nl = raw.indexOf("+CMGL:", pos);
        if (nl < 0) break;

        // Parse header line
        int eol = raw.indexOf('\n', nl);
        String hdr = (eol >= 0) ? raw.substring(nl, eol) : raw.substring(nl);

        SmsMsg& m = inbox[inboxCount];
        m.index = 0; m.sender[0] = 0; m.date[0] = 0; m.body[0] = 0;

        // index
        int c1 = hdr.indexOf(':');
        if (c1 >= 0) m.index = hdr.substring(c1+1).toInt();

        // "REC UNREAD" or "REC READ"
        m.read = (hdr.indexOf("\"REC READ\"") >= 0 || hdr.indexOf("\"STO SENT\"") >= 0);

        // sender (3rd quoted field)
        int q = c1;
        for (int i = 0; i < 2; i++) { q = hdr.indexOf('"', q+1); q = hdr.indexOf('"', q+1); }
        int q1 = hdr.indexOf('"', q+1), q2 = hdr.indexOf('"', q1+1);
        if (q1>=0 && q2>q1) strlcpy(m.sender, hdr.substring(q1+1,q2).c_str(), sizeof(m.sender));

        // date (5th quoted field)
        for (int i=0; i<2; i++) { q2 = hdr.indexOf('"', q2+1); q2 = hdr.indexOf('"', q2+1); }
        int dq1=q2; dq1 = hdr.indexOf('"', dq1+1);
        int dq2 = hdr.indexOf('"', dq1+1);
        if (dq1>=0 && dq2>dq1) strlcpy(m.date, hdr.substring(dq1+1,dq2).c_str(), sizeof(m.date));

        // body = next non-empty line after header
        pos = (eol >= 0) ? eol+1 : raw.length();
        while (pos < (int)raw.length() && (raw[pos]=='\r'||raw[pos]=='\n')) pos++;
        int bodyEnd = raw.indexOf('\n', pos);
        String bodyLine = (bodyEnd>=0) ? raw.substring(pos, bodyEnd) : raw.substring(pos);
        bodyLine.trim();
        strlcpy(m.body, bodyLine.c_str(), sizeof(m.body));

        pos = (bodyEnd>=0) ? bodyEnd+1 : raw.length();
        inboxCount++;
    }
}

// ── Inbox screen ──────────────────────────────────────────────────────────────
#define ROW_H    38
#define LIST_Y   (STATUS_H + 22)

static void drawInboxList() {
    M5.Display.fillRect(0, LIST_Y, DISP_W, DISP_H - LIST_Y, TFT_BLACK);
    if (inboxCount == 0) {
        M5.Display.setTextDatum(middle_center);
        M5.Display.setTextColor(0x4208, TFT_BLACK);
        M5.Display.setTextSize(1);
        M5.Display.drawString("No messages", DISP_W/2, LIST_Y + 60);
        return;
    }
    for (int i = 0; i < inboxCount && i < 5; i++) {
        int y = LIST_Y + i * ROW_H;
        uint32_t bg = inbox[i].read ? (uint32_t)0x1082 : (uint32_t)0x2104;
        M5.Display.fillRect(0, y, DISP_W, ROW_H-1, bg);
        M5.Display.drawFastHLine(0, y+ROW_H-1, DISP_W, 0x4208);

        // Unread dot
        if (!inbox[i].read) M5.Display.fillCircle(6, y+ROW_H/2, 4, TFT_CYAN);

        M5.Display.setTextDatum(top_left);
        M5.Display.setTextColor(TFT_WHITE, bg);
        M5.Display.setTextSize(1);
        M5.Display.drawString(inbox[i].sender, 16, y+4);

        // Body preview
        String preview = String(inbox[i].body).substring(0, 38);
        M5.Display.setTextColor(0xC618, bg);
        M5.Display.drawString(preview, 16, y+16);
    }
}

void enterSmsReadScreen() {
    M5.Display.fillScreen(TFT_BLACK);
    drawStatusBar();
    drawReadScreen();
}

static void drawReadScreen() {
    M5.Display.fillRect(0, STATUS_H, DISP_W, DISP_H-STATUS_H, TFT_BLACK);
    SmsMsg& m = inbox[selectedMsg];

    // Header with sender
    M5.Display.fillRect(0, STATUS_H, DISP_W, 22, 0x1082);
    M5.Display.fillRoundRect(2, STATUS_H+2, 56, 18, 4, 0x4208);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_WHITE, 0x4208); M5.Display.setTextSize(1);
    M5.Display.drawString("< BACK", 30, STATUS_H+11);
    M5.Display.setTextColor(TFT_CYAN, 0x1082);
    M5.Display.drawString(m.sender, DISP_W/2, STATUS_H+11);

    // Date
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextColor(0x4208, TFT_BLACK); M5.Display.setTextSize(1);
    M5.Display.drawString(m.date, 4, STATUS_H+26);

    // Body (wrap at ~52 chars per line, up to 8 lines)
    String body = m.body;
    int y = STATUS_H+40; int lineW = 52;
    while (body.length() > 0 && y < 200) {
        String seg = body.substring(0, lineW);
        body = (body.length() > (uint)lineW) ? body.substring(lineW) : "";
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.drawString(seg, 4, y);
        y += 12;
    }

    // Delete button
    M5.Display.fillRoundRect(20, 208, DISP_W-40, 28, 6, TFT_RED);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_WHITE, TFT_RED);
    M5.Display.drawString("DELETE", DISP_W/2, 222);
}

// ── Compose screen ────────────────────────────────────────────────────────────
#define COMP_NUM_Y   46
#define COMP_BODY_Y  70
#define COMP_PAD_Y  110
#define COMP_KEY_H   32   // (240-110)/4

static void drawComposeInputs() {
    // Number field
    uint32_t numBorder = (composeField==0) ? TFT_YELLOW : (uint32_t)0x4208;
    M5.Display.fillRect(0, COMP_NUM_Y, DISP_W, 22, TFT_BLACK);
    M5.Display.drawRect(0, COMP_NUM_Y, DISP_W, 22, numBorder);
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK); M5.Display.setTextSize(1);
    M5.Display.drawString("To: " + composeNum, 4, COMP_NUM_Y+11);

    // Body field
    uint32_t bodyBorder = (composeField==1) ? TFT_YELLOW : (uint32_t)0x4208;
    M5.Display.fillRect(0, COMP_BODY_Y, DISP_W, 38, TFT_BLACK);
    M5.Display.drawRect(0, COMP_BODY_Y, DISP_W, 38, bodyBorder);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    // Show last 2 lines of compose body
    String show = composeBody.length() > 72 ? composeBody.substring(composeBody.length()-72) : composeBody;
    M5.Display.drawString(show.substring(0, 36), 4, COMP_BODY_Y+4);
    if (show.length()>36) M5.Display.drawString(show.substring(36), 4, COMP_BODY_Y+18);
}

static void drawComposeKeypad() {
    // T9 numpad: rows 0-3 (same as USSD but with letter hints)
    // Row 0: 1(.,!?) 2(ABC) 3(DEF)
    // Row 1: 4(GHI)  5(JKL) 6(MNO)
    // Row 2: 7(PQRS) 8(TUV) 9(WXYZ)
    // Row 3: *(del)  0(spc) #(send)
    const char* hints[] = {".,!?","ABC","DEF","GHI","JKL","MNO","PQRS","TUV","WXYZ"};
    const char  nums[]  = {'1','2','3','4','5','6','7','8','9'};

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            int idx = r*3+c;
            int kx = c*KEY_W+1, ky = COMP_PAD_Y+r*COMP_KEY_H+1;
            int kw = KEY_W-2, kh = COMP_KEY_H-2;
            M5.Display.fillRoundRect(kx,ky,kw,kh,5,0x2945);
            char nBuf[2]={nums[idx],'\0'};
            M5.Display.setTextDatum(top_center);
            M5.Display.setTextColor(TFT_WHITE,0x2945); M5.Display.setTextSize(2);
            M5.Display.drawString(nBuf, kx+kw/2, ky+2);
            M5.Display.setTextSize(1);
            M5.Display.setTextColor(0xC618,0x2945);
            M5.Display.drawString(hints[idx], kx+kw/2, ky+kh-11);
        }
    }
    // Bottom row
    int ky = COMP_PAD_Y+3*COMP_KEY_H+1, kh = COMP_KEY_H-2;
    M5.Display.fillRoundRect(1,          ky, KEY_W-2, kh, 5, TFT_RED);
    M5.Display.fillRoundRect(KEY_W+1,    ky, KEY_W-2, kh, 5, 0x2945);
    M5.Display.fillRoundRect(KEY_W*2+1,  ky, KEY_W-2, kh, 5, 0x07E0);
    M5.Display.setTextDatum(middle_center); M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_RED);  M5.Display.drawString("DEL",  KEY_W/2,     ky+kh/2);
    M5.Display.setTextColor(TFT_WHITE, 0x2945);   M5.Display.drawString("0/SP", KEY_W+KEY_W/2, ky+kh/2);
    M5.Display.setTextColor(TFT_BLACK, 0x07E0);   M5.Display.drawString("SEND", KEY_W*2+KEY_W/2, ky+kh/2);
}

static void drawComposeScreen() {
    M5.Display.fillRect(0, STATUS_H, DISP_W, DISP_H-STATUS_H, TFT_BLACK);
    drawSmsHeader("Compose SMS");
    drawComposeInputs();
    drawComposeKeypad();
}

// ── SMS send AT ───────────────────────────────────────────────────────────────
static void doSendSMS() {
    if (composeNum.length()==0 || composeBody.length()==0) return;
    Serial.printf("[SMS >>] To:%s Msg:%s\n", composeNum.c_str(), composeBody.c_str());
    xSemaphoreTake(lteMutex, portMAX_DELAY);
    sendAT("AT+CMGF=1");
    lte.println("AT+CMGS=\"" + composeNum + "\"");
    // Wait for '>' prompt
    String prmpt; uint32_t t=millis();
    while (millis()-t<3000 && prmpt.indexOf('>')<0) while (lte.available()) prmpt+=(char)lte.read();
    lte.print(composeBody);
    lte.write(26); // Ctrl+Z
    String resp; t=millis();
    while (millis()-t<10000) while (lte.available()) resp+=(char)lte.read();
    xSemaphoreGive(lteMutex);
    Serial.printf("[SMS <<] %s\n", resp.c_str());

    bool ok = resp.indexOf("+CMGS:") >= 0;
    M5.Display.fillRect(0, STATUS_H, DISP_W, DISP_H-STATUS_H, TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    if (ok) { M5.Display.setTextColor(TFT_GREEN, TFT_BLACK); M5.Display.drawString("Sent!", DISP_W/2, DISP_H/2); }
    else    { M5.Display.setTextColor(TFT_RED,   TFT_BLACK); M5.Display.drawString("Failed", DISP_W/2, DISP_H/2); }
    M5.Display.setTextSize(1);
    delay(2000);
    currentScreen = SCREEN_MENU; screenDirty = true;
}

// ── T9 input ──────────────────────────────────────────────────────────────────
static void t9CommitPending() {
    // Nothing to commit — character is already in composeBody
    t9Key = -1; t9Count = 0;
}

static void t9HandleKey(int key) {
    // key: 0-9 for digit, -2=DEL, -3=SPC
    if (key == -2) {
        t9CommitPending();
        if (composeBody.length()>0) composeBody.remove(composeBody.length()-1);
        drawComposeInputs(); return;
    }
    if (key == -3) {
        t9CommitPending();
        composeBody += ' '; drawComposeInputs(); return;
    }
    const char* chars = T9_MAP[key];
    int n = strlen(chars);
    if (t9Key == key && millis()-t9Time < T9_TIMEOUT) {
        t9Count = (t9Count+1) % n;
        if (composeBody.length()>0) composeBody.remove(composeBody.length()-1);
    } else {
        t9CommitPending();
        t9Count = 0;
    }
    composeBody += chars[t9Count];
    t9Key = key; t9Time = millis();
    drawComposeInputs();
}

// ── Public API ────────────────────────────────────────────────────────────────
void enterSmsInboxScreen() {
    M5.Display.fillScreen(TFT_BLACK);
    drawStatusBar();
    drawSmsHeader("SMS Inbox");
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK); M5.Display.setTextSize(1);
    M5.Display.drawString("Loading...", DISP_W/2, DISP_H/2);
    loadInbox();
    lte_clearUnread();
    drawInboxList();
}

void enterSmsComposeScreen() {
    composeNum   = ""; composeBody = "";
    composeField = 0;
    t9Key = -1; t9Count = 0;
    M5.Display.fillScreen(TFT_BLACK);
    drawStatusBar();
    drawComposeScreen();
}

void handleSmsTouch(int tx, int ty) {
    // ── INBOX ─────────────────────────────────────────────────────────────────
    if (currentScreen == SCREEN_SMS_INBOX) {
        // Header back button
        if (ty >= STATUS_H+2 && ty <= STATUS_H+20 && tx <= 58) {
            beep(600,80); currentScreen=SCREEN_MENU; screenDirty=true; return;
        }
        // Tap on a message
        int row = (ty - LIST_Y) / ROW_H;
        if (row >= 0 && row < inboxCount) {
            beep(); selectedMsg=row; currentScreen=SCREEN_SMS_READ; screenDirty=true;
        }
        return;
    }

    // ── READ ──────────────────────────────────────────────────────────────────
    if (currentScreen == SCREEN_SMS_READ) {
        // Back
        if (ty >= STATUS_H+2 && ty <= STATUS_H+20 && tx <= 58) {
            beep(600,80); currentScreen=SCREEN_SMS_INBOX; screenDirty=true; return;
        }
        // Delete
        if (ty >= 208 && ty <= 236) {
            beep(800,80);
            String cmd = String("AT+CMGD=") + inbox[selectedMsg].index;
            sendATLocked(cmd.c_str());
            currentScreen=SCREEN_SMS_INBOX; screenDirty=true;
        }
        return;
    }

    // ── COMPOSE ───────────────────────────────────────────────────────────────
    if (currentScreen == SCREEN_SMS_COMPOSE) {
        // Back
        if (ty >= STATUS_H+2 && ty <= STATUS_H+20 && tx <= 58) {
            beep(600,80); currentScreen=SCREEN_MENU; screenDirty=true; return;
        }
        // Toggle active field
        if (ty >= COMP_NUM_Y && ty < COMP_BODY_Y) { composeField=0; drawComposeInputs(); return; }
        if (ty >= COMP_BODY_Y && ty < COMP_PAD_Y) { composeField=1; drawComposeInputs(); return; }

        // Keypad
        if (ty < COMP_PAD_Y) return;
        int row=(ty-COMP_PAD_Y)/COMP_KEY_H, col=tx/KEY_W;
        if (row < 3) {
            int key = row*3+col+1; // 1-9
            beep();
            if (composeField==0) {
                char d='0'+key; composeNum+=d; drawComposeInputs();
            } else {
                t9HandleKey(key);
            }
        } else if (row == 3) {
            if (col==0) { // DEL
                beep(800,50);
                if (composeField==0) { if(composeNum.length()>0) composeNum.remove(composeNum.length()-1); drawComposeInputs(); }
                else t9HandleKey(-2);
            } else if (col==1) { // 0 / SPC
                beep();
                if (composeField==0) { composeNum += '0'; drawComposeInputs(); }
                else t9HandleKey(-3);
            } else if (col==2) { // SEND
                beep(1400,80); t9CommitPending(); doSendSMS();
            }
        }
    }
}

void tickSmsScreen() {
    if (currentScreen != SCREEN_SMS_COMPOSE) return;
    if (t9Key >= 0 && millis()-t9Time >= T9_TIMEOUT) {
        t9CommitPending(); // lock in current char, next key starts fresh
    }
}
