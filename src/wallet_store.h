#pragma once
#include <Arduino.h>

#define WALLET_MAX_ENTRIES 24
#define WALLET_NAME_LEN    32
#define WALLET_USER_LEN    64
#define WALLET_PASS_LEN    64

struct WalletEntry {
    char name[WALLET_NAME_LEN];
    char username[WALLET_USER_LEN];
    char password[WALLET_PASS_LEN];
    bool pinned;   // one-tap password shortcut
};

bool wallet_load();
bool wallet_save();

int  wallet_count();
const WalletEntry* wallet_entry(int idx);

bool wallet_add(const WalletEntry* e);
bool wallet_remove(int idx);
bool wallet_set_pinned(int idx, bool pinned);
bool wallet_storage_full();
int  wallet_pinned_count();

bool wallet_ready();
