#include "clip_bridge.h"
#include "ble_hid.h"
#include "config.h"
#include "sd_card.h"
#include <SD.h>
#include <stdio.h>
#include <string.h>

static const uint8_t FLAG_FIRST = 0x01;
static const uint8_t FLAG_LAST  = 0x02;
static const uint32_t TIMEOUT_MS = 8000;

static volatile bool s_waiting = false;
static volatile bool s_ready = false;
static volatile uint32_t s_t0 = 0;
static char s_body[256];
static uint16_t s_got = 0;

void clip_cancel() {
    s_waiting = false;
    s_ready = false;
    s_got = 0;
    s_body[0] = 0;
}

bool clip_busy() { return s_waiting && !s_ready; }

void clip_begin_request() {
    s_got = 0;
    s_body[0] = 0;
    s_ready = false;
    s_waiting = true;
    s_t0 = millis();
    ble_hid_clip_request();
}

void clip_on_host_report(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;
    // GATT may include the report ID; skip it if present.
    if (data[0] == 0x04 && len >= 3) {
        data++;
        len--;
    }
    if (len < 2) return;
    uint8_t flags = data[0];
    if (flags == 0xF1) return;  // echo of our own GetFeature request

    uint8_t n = data[1];
    if (n > 30) n = 30;
    if ((size_t)(2 + n) > len) n = (uint8_t)(len - 2);

    if (flags & FLAG_FIRST) {
        s_got = 0;
        ble_hid_clip_ack();
    }
    if (!s_waiting) return;

    if (s_got + n > sizeof(s_body) - 1)
        n = (uint8_t)(sizeof(s_body) - 1 - s_got);
    if (n) memcpy(s_body + s_got, data + 2, n);
    s_got = (uint16_t)(s_got + n);

    if (flags & FLAG_LAST) {
        s_body[s_got] = 0;
        while (s_got && (s_body[s_got - 1] == '\n' || s_body[s_got - 1] == '\r'))
            s_body[--s_got] = 0;
        s_ready = true;
    }
}

bool clip_poll(char* buf, size_t buflen) {
    if (!s_waiting || !buf || buflen < 2) return false;
    if (s_ready) {
        size_t n = s_got;
        if (n >= buflen) n = buflen - 1;
        memcpy(buf, s_body, n);
        buf[n] = 0;
        clip_cancel();
        return true;
    }
    if (millis() - s_t0 > TIMEOUT_MS) {
        clip_cancel();
        buf[0] = 0;
        return true;
    }
    return false;
}

static bool s_helper_live = false;

static void clip_helper_ps1_path(char* out, size_t n) {
    strlcpy(out, CLIP_HELPER_PS1, n);
    if (!sd_card_mount_ok() || n < 8) return;
    File f = SD.open(CLIP_HELPER_PATH_FILE, FILE_READ);
    if (!f) return;
    char buf[200];
    size_t got = f.readBytes(buf, sizeof(buf) - 1);
    f.close();
    buf[got] = 0;
    char* s = buf;
    while (*s == ' ' || *s == '\t' || *s == '"' || *s == '\'') s++;
    size_t len = strlen(s);
    while (len && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' ||
                   s[len - 1] == '\n' || s[len - 1] == '"' || s[len - 1] == '\''))
        s[--len] = 0;
    if (len >= 4) strlcpy(out, s, n);
}

void clip_helper_start(uint8_t quit_after) {
    if (!ble_hid_connected()) return;
    if (quit_after < 1) quit_after = 1;

    char path[200];
    clip_helper_ps1_path(path, sizeof(path));

    // One Win+R command: do not type into an already-open PowerShell prompt.
    ble_hid_combo(KEY_LEFT_GUI, 'r');
    delay(500);

    char cmd[360];
    snprintf(cmd, sizeof(cmd),
             "powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Minimized -File \"%s\" -QuitAfter %u",
             path, (unsigned)quit_after);
    ble_hid_type(cmd);
    delay(120);
    ble_hid_key(KEY_RETURN);

    s_helper_live = true;
    delay(CLIP_HELPER_BOOT_MS);
}

void clip_helper_close() {
    if (!s_helper_live || !ble_hid_connected()) {
        s_helper_live = false;
        return;
    }
    s_helper_live = false;
    ble_hid_combo(KEY_LEFT_GUI, 'r');
    delay(400);
    ble_hid_type("taskkill /F /FI \"WINDOWTITLE eq WalletRemoteHelper*\"");
    delay(80);
    ble_hid_key(KEY_RETURN);
    delay(300);
}
