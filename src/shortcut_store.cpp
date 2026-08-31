#include "shortcut_store.h"
#include "sd_card.h"
#include "config.h"
#include <SD.h>
#include <ArduinoJson.h>

static KbShortcut s_items[KB_MAX_SHORTCUTS];
static int s_count = 0;
static bool s_ready = false;

bool kb_ready() { return s_ready; }
int kb_count() { return s_count; }
bool kb_storage_full() { return s_count >= KB_MAX_SHORTCUTS; }

const KbShortcut* kb_entry(int idx) {
    if (idx < 0 || idx >= s_count) return nullptr;
    return &s_items[idx];
}

static void write_default() {
    File f = SD.open(KB_SHORTCUTS_FILE, FILE_WRITE);
    if (!f) return;
    f.print("{\"version\":1,\"items\":[]}");
    f.close();
}

bool kb_load() {
    s_count = 0;
    s_ready = false;
    if (!sd_card_mount_ok()) return false;

    if (!SD.exists(KB_SHORTCUTS_FILE))
        write_default();

    File f = SD.open(KB_SHORTCUTS_FILE, FILE_READ);
    if (!f) return false;

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("kb: JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonArray arr = doc["items"].as<JsonArray>();
    if (arr.isNull()) return false;

    for (JsonObject o : arr) {
        if (s_count >= KB_MAX_SHORTCUTS) break;
        KbShortcut& s = s_items[s_count++];
        memset(&s, 0, sizeof(s));
        strlcpy(s.name, o["name"] | "", sizeof(s.name));
        strlcpy(s.text, o["text"] | "", sizeof(s.text));
    }

    s_ready = true;
    Serial.printf("kb: loaded %d shortcuts\n", s_count);
    return true;
}

bool kb_save() {
    if (!sd_card_mount_ok()) return false;

    DynamicJsonDocument doc(4096);
    doc["version"] = 1;
    JsonArray arr = doc.createNestedArray("items");
    for (int i = 0; i < s_count; ++i) {
        JsonObject o = arr.createNestedObject();
        o["name"] = s_items[i].name;
        o["text"] = s_items[i].text;
    }

    if (SD.exists(KB_SHORTCUTS_FILE)) SD.remove(KB_SHORTCUTS_FILE);
    File f = SD.open(KB_SHORTCUTS_FILE, FILE_WRITE);
    if (!f) return false;
    if (serializeJson(doc, f) == 0) {
        f.close();
        return false;
    }
    f.close();
    s_ready = true;
    return true;
}

bool kb_add(const KbShortcut* s) {
    if (!s || kb_storage_full()) return false;
    s_items[s_count++] = *s;
    return kb_save();
}

bool kb_remove(int idx) {
    if (idx < 0 || idx >= s_count) return false;
    for (int i = idx; i < s_count - 1; ++i)
        s_items[i] = s_items[i + 1];
    s_count--;
    return kb_save();
}
