#pragma once
#include <Arduino.h>

// BLE HID clipboard bridge. The board launches tools/clip-helper.ps1 on the
// PC over Bluetooth (Win+R → PowerShell), then reads the clipboard through
// vendor HID report ID 4.

void clip_begin_request();
// Returns true when a reply is ready. buf is always 0-terminated.
bool clip_poll(char* buf, size_t buflen);
void clip_cancel();
bool clip_busy();

// Called from the BLE HID stack when the PC writes a vendor report.
void clip_on_host_report(const uint8_t* data, size_t len);

// Open PowerShell, run the stored helper, minimize it, wait until it is up.
void clip_helper_start(uint8_t quit_after);
// Close the helper PowerShell window (taskkill by window title).
void clip_helper_close();
