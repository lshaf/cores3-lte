#pragma once
#include <Arduino.h>

enum Screen {
    SCREEN_MENU,
    SCREEN_USSD,
    SCREEN_SMS_INBOX,
    SCREEN_SMS_COMPOSE,
    SCREEN_SMS_READ
};

extern volatile Screen currentScreen;
extern volatile bool   screenDirty;
extern volatile bool   menuDirty;
extern volatile bool   statusDirty;
