#pragma once
#include <Arduino.h>
#include <BleKeyboard.h>   // KEY_* constants for HID

void ble_hid_begin();
void ble_hid_end();
bool ble_hid_connected();

void ble_hid_type(const char* text);
void ble_hid_key(uint8_t key);
void ble_hid_media(const MediaKeyReport key);   // KEY_MEDIA_* are 2-byte reports
void ble_hid_combo(uint8_t mod, uint8_t key);
void ble_hid_combo2(uint8_t m1, uint8_t m2, uint8_t key);
void ble_hid_release_all();

#define MOUSE_LEFT   0x01
#define MOUSE_RIGHT  0x02
#define MOUSE_MIDDLE 0x04
void ble_hid_mouse_move(int8_t x, int8_t y, int8_t wheel = 0);
void ble_hid_mouse_click(uint8_t buttons);

void ble_hid_clip_request();
void ble_hid_clip_ack();
