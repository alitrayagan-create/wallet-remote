#pragma once
#include <Arduino.h>

// Mount the CYD microSD card (VSPI, GPIO 5 CS). Safe to call multiple times.
bool sd_card_init();

bool sd_card_present();
bool sd_card_mount_ok();

// Free space in bytes (0 if unavailable).
uint64_t sd_card_free_bytes();

// Human-readable size string, e.g. "1.2 GB free".
void sd_card_free_str(char* buf, size_t len);
