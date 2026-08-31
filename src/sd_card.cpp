#include "sd_card.h"
#include "config.h"
#include <SPI.h>
#include <SD.h>

static bool s_hw = false;      // card detected at slot
static bool s_mounted = false;
static SPIClass s_sdSPI(VSPI);

bool sd_card_init() {
    s_hw = false;
    s_mounted = false;

    s_sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, s_sdSPI, 4000000)) {
        Serial.println("SD: no card or mount failed");
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        SD.end();
        return false;
    }

    s_hw = true;
    s_mounted = true;
    Serial.printf("SD: mounted (%s)\n",
                  cardType == CARD_MMC ? "MMC" :
                  cardType == CARD_SD ? "SDSC" :
                  cardType == CARD_SDHC ? "SDHC" : "unknown");
    return true;
}

bool sd_card_present() { return s_hw; }
bool sd_card_mount_ok() { return s_mounted; }

uint64_t sd_card_free_bytes() {
    if (!s_mounted) return 0;
    return SD.totalBytes() > SD.usedBytes()
               ? (SD.totalBytes() - SD.usedBytes()) : 0;
}

void sd_card_free_str(char* buf, size_t len) {
    uint64_t b = sd_card_free_bytes();
    if (b >= 1024ULL * 1024 * 1024)
        snprintf(buf, len, "%.1f GB free", b / (1024.0 * 1024 * 1024));
    else if (b >= 1024ULL * 1024)
        snprintf(buf, len, "%.1f MB free", b / (1024.0 * 1024));
    else if (b > 0)
        snprintf(buf, len, "%llu KB free", (unsigned long long)(b / 1024));
    else
        snprintf(buf, len, "unknown");
}
