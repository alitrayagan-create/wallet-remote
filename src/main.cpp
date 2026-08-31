#include <Arduino.h>
#include <lvgl.h>
#include <esp_system.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "ui.h"

bool ui_first_frame_done();   // from ui.cpp

void setup() {
    // Disable the brownout detector. On the CYD, the current spike when the BLE
    // radio powers up can trip the detector and reset the board on weak USB
    // power. This keeps it running; still, use a good 5V/2A supply + cable.
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    delay(100);                 // small settle time helps some CYD units init cleanly
    Serial.println("\n\n=== Wallet Remote v2.2 ===");
    Serial.printf("Reset reason: %d (1=poweron 4=panic 5/6/7=wdt 9=brownout 12=sw)\n",
                  (int)esp_reset_reason());

    ui_init();                  // backlight stays OFF here (no boot flash)

    // Draw the first frame while the panel is still dark, then fade the
    // backlight on so the user never sees a white/black flash.
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
