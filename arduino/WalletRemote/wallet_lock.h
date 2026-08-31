#pragma once

// Wallet access password (SHA-256 hash stored in ESP32 NVS, not on the SD card).
bool wallet_lock_is_set();
bool wallet_lock_set(const char* password);   // create or replace
bool wallet_lock_check(const char* password);

#define WALLET_LOCK_MIN 4
#define WALLET_LOCK_MAX 32
