#include "ble_hid.h"
#include "config.h"
#include "clip_bridge.h"
#include <BleKeyboard.h>

static BleKeyboard s_kb(BLE_DEVICE_NAME, BLE_MANUFACTURER, 100);
static bool s_on = false;

void ble_hid_begin() {
    if (!s_on) {
        Serial.printf("BLE begin: free heap = %u, largest block = %u\n",
                      (unsigned)ESP.getFreeHeap(),
                      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        s_kb.setClipOutHandler(clip_on_host_report);
        s_kb.begin();
        s_on = true;
        Serial.printf("BLE begin done: free heap = %u\n", (unsigned)ESP.getFreeHeap());
    }
}

void ble_hid_end() {
    if (s_on) {
        s_kb.end();
        s_on = false;
    }
}

bool ble_hid_connected() { return s_on && s_kb.isConnected(); }

void ble_hid_type(const char* text) {
    if (!ble_hid_connected() || !text) return;
    s_kb.print(text);
}

void ble_hid_key(uint8_t key) {
    if (!ble_hid_connected()) return;
    s_kb.write(key);
}

void ble_hid_media(const MediaKeyReport key) {
    if (!ble_hid_connected()) return;
    s_kb.write(key);
}

void ble_hid_combo(uint8_t mod, uint8_t key) {
    if (!ble_hid_connected()) return;
    s_kb.press(mod);
    s_kb.press(key);
    delay(20);
    s_kb.releaseAll();
}

void ble_hid_combo2(uint8_t m1, uint8_t m2, uint8_t key) {
    if (!ble_hid_connected()) return;
    s_kb.press(m1);
    s_kb.press(m2);
    s_kb.press(key);
    delay(20);
    s_kb.releaseAll();
}

void ble_hid_release_all() {
    if (s_on) s_kb.releaseAll();
}

void ble_hid_mouse_move(int8_t x, int8_t y, int8_t wheel) {
    if (!ble_hid_connected()) return;
    s_kb.mouseMove(x, y, wheel);
}

void ble_hid_mouse_click(uint8_t buttons) {
    if (!ble_hid_connected()) return;
    s_kb.mouseClick(buttons);
}

void ble_hid_clip_request() {
    if (s_on) s_kb.sendClipRequest();
}

void ble_hid_clip_ack() {
    if (s_on) s_kb.clearClipRequest();
}
