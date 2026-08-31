#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Wallet Remote hardware configuration  (ESP32-2432S028 / Cheap Yellow Display)
// ---------------------------------------------------------------------------

static const uint16_t SCREEN_W = 240;
static const uint16_t SCREEN_H = 320;

#define UI_ROTATION 0

// --- Backlight ---
#define PIN_BACKLIGHT 21
#define BL_LEDC_CHANNEL 0
#define BL_LEDC_FREQ 5000
#define BL_LEDC_RES 8

// --- Touch (XPT2046) ---
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

#define TOUCH_RAW_MIN_X 200
#define TOUCH_RAW_MAX_X 3700
#define TOUCH_RAW_MIN_Y 240
#define TOUCH_RAW_MAX_Y 3800

// Orientation correction for the bit-banged touch. Flip if a press registers
// on the opposite side. Set to 0/1.
#define TOUCH_INVERT_X 1
#define TOUCH_INVERT_Y 0
#define TOUCH_SWAP_XY  1

// Set to 1 to print raw touch + reset reason on serial for debugging.
#define TOUCH_DEBUG 0

// --- microSD card (VSPI) ---
#define SD_MISO 19
#define SD_MOSI 23
#define SD_SCK  18
#define SD_CS   5

// --- On-board RGB LED (common anode: drive pin LOW = that color ON) ---
#define PIN_LED_R 4
#define PIN_LED_G 16
#define PIN_LED_B 17
#define LEDC_RGB_R 1
#define LEDC_RGB_G 2
#define LEDC_RGB_B 3

#define WALLET_DATA_FILE     "/data.json"
#define KB_SHORTCUTS_FILE    "/shortcuts.json"
#define CLIP_HELPER_PATH_FILE "/helper-path.txt"

// --- BLE HID ---
#define BLE_DEVICE_NAME  "Wallet Remote"
#define BLE_MANUFACTURER "Wallet Remote"

// Path the board types into PowerShell. Override with /helper-path.txt on the SD card.
#define CLIP_HELPER_PS1 "C:\\WalletRemote\\clip-helper.ps1"
#define CLIP_HELPER_BOOT_MS 2500
