#pragma once
void enterSmsInboxScreen();
void enterSmsReadScreen();
void enterSmsComposeScreen();
void handleSmsTouch(int tx, int ty);
void tickSmsScreen();   // call every loop() tick for T9 timeout
