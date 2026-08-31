#pragma once
#include <Arduino.h>

#define KB_MAX_SHORTCUTS 16
#define KB_NAME_LEN      32
#define KB_TEXT_LEN      96

struct KbShortcut {
    char name[KB_NAME_LEN];
    char text[KB_TEXT_LEN];
};

bool kb_load();
bool kb_save();
int  kb_count();
const KbShortcut* kb_entry(int idx);
bool kb_add(const KbShortcut* s);
bool kb_remove(int idx);
bool kb_storage_full();
bool kb_ready();
