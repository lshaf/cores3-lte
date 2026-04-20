#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define LTE_RX   1
#define LTE_TX   7
#define LTE_BAUD 115200

extern HardwareSerial    lte;
extern SemaphoreHandle_t lteMutex;
extern volatile int      sigBars;
extern char              opName[20];
extern volatile int      unreadSMS;

int    rssiToBars(int rssi);
String sendAT(const char* cmd, uint32_t timeout = 2000);
String sendATLocked(const char* cmd, uint32_t timeout = 2000);
bool   lteInit();
void   startSignalTask();
void   lte_onURC(const String& urc);
void   lte_clearUnread();
