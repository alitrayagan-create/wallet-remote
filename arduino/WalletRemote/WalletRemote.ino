/*
 * Wallet Remote — Arduino firmware for ESP32-2432S028 (Cheap Yellow Display)
 *
 * Open this file in Arduino IDE (board: ESP32 Dev Module, partition: Huge APP)
 * or build with PlatformIO:  python -m platformio run -e cyd
 *
 * Libraries (Library Manager):
 *   lvgl, TFT_eSPI, ArduinoJson, NimBLE-Arduino
 *
 * Compiler flags live in build_opt.h (same folder).
 */

#include <Arduino.h>
#include <lvgl.h>
#include <esp_system.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "ui.h"

bool ui_first_frame_done();

void setup() {
    // Disable brownout. BLE radio startup can reset the CYD on a weak USB supply.
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    delay(100);
    Serial.println("\n\n=== Wallet Remote v2.2 ===");
    Serial.printf("Reset reason: %d (1=poweron 4=panic 5/6/7=wdt 9=brownout 12=sw)\n",
                  (int)esp_reset_reason());

    ui_init();

    uint32_t start = millis();
    while (!ui_first_frame_done() && millis() - start < 1500) {
        lv_timer_handler();
        delay(5);
    }
    backlight_fade_on();
}

void loop() {
    ui_tick();
    delay(5);
}
