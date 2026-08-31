#include "wallet_store.h"
#include "sd_card.h"
#include "config.h"
#include <SD.h>
#include <ArduinoJson.h>

static WalletEntry s_entries[WALLET_MAX_ENTRIES];
static int s_count = 0;
static bool s_ready = false;

bool wallet_ready() { return s_ready; }
int wallet_count() { return s_count; }

const WalletEntry* wallet_entry(int idx) {
    if (idx < 0 || idx >= s_count) return nullptr;
    return &s_entries[idx];
}

bool wallet_storage_full() { return s_count >= WALLET_MAX_ENTRIES; }

int wallet_pinned_count() {
    int n = 0;
    for (int i = 0; i < s_count; ++i)
        if (s_entries[i].pinned) ++n;
    return n;
}

static void write_default_json() {
    File f = SD.open(WALLET_DATA_FILE, FILE_WRITE);
    if (!f) return;
    f.print("{\"version\":2,\"entries\":[]}");
    f.close();
}

bool wallet_load() {
    s_count = 0;
    s_ready = false;
    if (!sd_card_mount_ok()) return false;

    if (!SD.exists(WALLET_DATA_FILE))
        write_default_json();

    File f = SD.open(WALLET_DATA_FILE, FILE_READ);
    if (!f) return false;

    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("wallet: JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonArray arr = doc["entries"].as<JsonArray>();
    if (arr.isNull()) return false;

    for (JsonObject o : arr) {
        if (s_count >= WALLET_MAX_ENTRIES) break;
        WalletEntry& e = s_entries[s_count++];
        memset(&e, 0, sizeof(e));
        strlcpy(e.name, o["name"] | "", sizeof(e.name));
        strlcpy(e.username, o["username"] | "", sizeof(e.username));
        strlcpy(e.password, o["password"] | "", sizeof(e.password));
        e.pinned = o["pinned"] | false;
    }

    s_ready = true;
    Serial.printf("wallet: loaded %d entries\n", s_count);
    return true;
}

bool wallet_save() {
    if (!sd_card_mount_ok()) return false;

    DynamicJsonDocument doc(8192);
    doc["version"] = 2;
    JsonArray arr = doc.createNestedArray("entries");
    for (int i = 0; i < s_count; ++i) {
        JsonObject o = arr.createNestedObject();
        o["name"] = s_entries[i].name;
        o["username"] = s_entries[i].username;
        o["password"] = s_entries[i].password;
        if (s_entries[i].pinned)
            o["pinned"] = true;
    }

    if (SD.exists(WALLET_DATA_FILE)) SD.remove(WALLET_DATA_FILE);
    File f = SD.open(WALLET_DATA_FILE, FILE_WRITE);
    if (!f) return false;
    if (serializeJson(doc, f) == 0) {
        f.close();
        return false;
    }
    f.close();
    s_ready = true;
    return true;
}

bool wallet_add(const WalletEntry* e) {
    if (!e || wallet_storage_full()) return false;
    s_entries[s_count++] = *e;
    return wallet_save();
}

bool wallet_remove(int idx) {
    if (idx < 0 || idx >= s_count) return false;
    for (int i = idx; i < s_count - 1; ++i)
        s_entries[i] = s_entries[i + 1];
    s_count--;
    return wallet_save();
}

bool wallet_set_pinned(int idx, bool pinned) {
    if (idx < 0 || idx >= s_count) return false;
    s_entries[idx].pinned = pinned;
    return wallet_save();
}
