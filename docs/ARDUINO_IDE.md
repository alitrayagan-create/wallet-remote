# Arduino firmware

Wallet Remote is **Arduino-ESP32** firmware. The sketch is:

**[`arduino/WalletRemote/WalletRemote.ino`](../arduino/WalletRemote/WalletRemote.ino)**

`setup()` and `loop()` live in that file. The rest of the firmware is `.cpp` / `.h` in the same folder (wallet, macro pad, BLE keyboard, display).

## Build with PlatformIO (recommended)

Same as the main README:

```powershell
python -m platformio run -e cyd
powershell -ExecutionPolicy Bypass -File scripts\rebuild.ps1
powershell -ExecutionPolicy Bypass -File scripts\flash.ps1 COM4
```

`platformio.ini` sets `src_dir = arduino/WalletRemote` so this sketch is what gets compiled.

## Build with Arduino IDE

1. Install [Arduino IDE 2](https://www.arduino.cc/en/software) and the **esp32** board package (Espressif).
2. Library Manager — install:
   - **lvgl** (8.3.x or 8.4.x)
   - **TFT_eSPI**
   - **ArduinoJson** (6.x)
   - **NimBLE-Arduino**
3. File → Open `arduino/WalletRemote/WalletRemote.ino`
4. Tools:
   - Board: **ESP32 Dev Module**
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
   - Upload Speed: **921600** (or 460800 if upload fails)
5. `build_opt.h` in the sketch folder holds the CYD display and NimBLE flags. Do not delete it.
6. Sketch → Upload

If the image is garbled, edit `build_opt.h` and `platformio.ini` together (`TFT_INVERSION_ON`, or switch to `ILI9341_DRIVER`). Touch mapping is in `config.h`.
