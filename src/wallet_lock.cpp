#include "wallet_lock.h"
#include <Preferences.h>
#include <string.h>
#include "mbedtls/sha256.h"

static const char* NVS_NS = "wlock";
static const char* NVS_KEY = "hash";

static void hash_password(const char* password, uint8_t out[32]) {
    mbedtls_sha256((const unsigned char*)password, strlen(password), out, 0);
}

bool wallet_lock_is_set() {
    Preferences p;
    if (!p.begin(NVS_NS, true)) return false;
    bool set = p.isKey(NVS_KEY);
    p.end();
    return set;
}

bool wallet_lock_set(const char* password) {
    if (!password) return false;
    size_t n = strlen(password);
    if (n < WALLET_LOCK_MIN || n > WALLET_LOCK_MAX) return false;

    uint8_t hash[32];
    hash_password(password, hash);

    Preferences p;
    if (!p.begin(NVS_NS, false)) return false;
    size_t wrote = p.putBytes(NVS_KEY, hash, 32);
    p.end();
    return wrote == 32;
}

bool wallet_lock_check(const char* password) {
    if (!password) return false;

    uint8_t got[32], want[32];
    hash_password(password, got);

    Preferences p;
    if (!p.begin(NVS_NS, true)) return false;
    size_t n = p.getBytes(NVS_KEY, want, 32);
    p.end();
    if (n != 32) return false;
    return memcmp(got, want, 32) == 0;
}
