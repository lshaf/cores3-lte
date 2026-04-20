# CoreS3 LTE

M5Stack CoreS3 touch UI app for USSD dialing and SMS over a SIM7600G LTE module (COM.X).

## Hardware

| Part | Details |
|------|---------|
| MCU | M5Stack CoreS3 (ESP32-S3) |
| LTE module | M5Stack COM.X LTE (SIM7600G-H) |
| Connection | UART — RX pin 1, TX pin 7 |

Attach the COM.X module to the CoreS3 bottom port. No wiring required beyond that.

## Features

- **USSD Dialer** — dial any USSD code, live 60 s countdown overlay, session reply support
- **SMS Inbox** — fetch all messages, read, delete
- **Compose SMS** — T9 multi-tap input for message body, direct digit entry for phone number
- **Signal bar** — 4-bar RSSI indicator + operator name, updated in background (FreeRTOS task, never blocks UI)
- **Unread badge** — red count badge on SMS Inbox menu item when new messages arrive

## Requirements

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- M5Unified library (fetched automatically via `lib_deps`)
- SIM card inserted in the COM.X module

## Setup

1. Clone the repo:
   ```
   git clone git@github.com:lshaf/cores3-lte.git
   cd cores3-lte
   ```

2. Open in VS Code with the PlatformIO extension, or use the CLI.

3. Check pin definitions in `src/lte.h` if your wiring differs:
   ```cpp
   #define LTE_RX  1
   #define LTE_TX  7
   ```

4. Build and flash:
   ```
   pio run --target upload
   ```

5. Open serial monitor at 115200 baud to see AT command logs:
   ```
   pio device monitor
   ```

## Project structure

```
src/
  main.cpp          — setup/loop, screen dispatcher, URC drain
  app.h             — shared Screen enum and volatile dirty flags
  lte.h / lte.cpp   — AT helpers, signal background task, URC handler
  ui.h / ui.cpp     — status bar, signal bars, beep
  screen_menu.*     — main menu (USSD, SMS Inbox, Compose SMS)
  screen_ussd.*     — USSD dialer + session popup
  screen_sms.*      — SMS inbox, read/delete, compose with T9
```
