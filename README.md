# Wallet Remote

Firmware for the **ESP32-2432S028** (Cheap Yellow Display): a pocket **password wallet** and **Windows Bluetooth keyboard** on a 2.8" touchscreen.

Pair the board as **`Wallet Remote`**. Logins and Type shortcuts live on a FAT32 microSD card.

**[Full tutorial: flash, pair, and every menu with pictures →](docs/TUTORIAL.md)**

<p align="center">
  <img src="docs/images/home.png" alt="Home menu" width="180">
  <img src="docs/images/wallet-home.png" alt="Password Wallet" width="180">
  <img src="docs/images/macropad.png" alt="Macro Pad" width="180">
  <img src="docs/images/settings.png" alt="Settings" width="180">
</p>

## Features

- **Password Wallet** — add, type, copy, pin, and delete logins from the screen
- **Capture from PC** — tap once; the board opens PowerShell, copies username and password over Bluetooth, then closes the helper
- **Macro Pad** — Edit, Media, System, Apps, Power, Type, and Mouse
- **Type shortcuts** — save named snippets (including “copy save from PC”)
- **Settings** — brightness plus an RGB rainbow cycle (or a solid color)
- BLE stays paired across apps (NimBLE)

## Hardware

- ESP32-2432S028 (240×320 TFT + XPT2046 touch)
- FAT32 microSD (recommended)
- Windows 10/11 PC with Bluetooth

This build uses the **ST7789** panel driver. If the image is garbled or inverted, see [Board variant](#board-variant).

## Flash

See the **[tutorial](docs/TUTORIAL.md)** for pictures and a full walkthrough.

One file contains bootloader, partitions, and app: **`dist/wallet-remote.bin`**. Write it at **`0x0`**.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\rebuild.ps1
powershell -ExecutionPolicy Bypass -File scripts\flash.ps1 COM5
```

Replace `COM5` with your port:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
```

Do not flash the smaller `firmware.bin` at `0x0` — that produces a black screen.

## One-time PC setup

The board cannot read the Windows clipboard by itself. On first setup, install the helper to the path the firmware types:

```powershell
powershell -ExecutionPolicy Bypass -File tools\install-helper.ps1
```

That copies the helper to `C:\WalletRemote\`. You do not need to start it by hand after that.

To use a different folder, put one line in `/helper-path.txt` on the SD card (see `sd_template/helper-path.example.txt`).

## Using the Password Wallet

1. Pair **Wallet Remote** in Windows (Settings → Bluetooth).
2. Open **Password Wallet**. The first time, set an access password (4+ characters). That hash is stored on the ESP32, not on the SD card. Change it under Wallet → Settings.
3. Power the board from USB, a power bank, or any 5 V source — a data cable is not required after flashing.
4. **Add user and pass**: click the username box on the PC, leave that window focused, tap the button. The board starts the helper, copies username then password, closes PowerShell, and asks for a shortcut name.
5. **Type → Copy save from PC** copies selected text the same way.
6. Open an entry:
   - **Type username / password / login** — types into the focused PC field
   - **Copy password** — types it, then Ctrl+A / Ctrl+C so it is on the clipboard
   - **Pin as shortcut** — one-tap password from the Shortcuts tile
   - **Delete entry** — removes it from the list and the SD card

Browsers often block copying from hidden password fields. Show the password on the site, or use **Type manually**.

## Using the Macro Pad

| Category | What it sends |
|----------|----------------|
| Edit | Copy, Cut, Paste, Undo, Redo, Save, Select All, Find |
| Media | Play/Pause, tracks, volume, mute + volume slider |
| System | Snip, screen record, lock, desktop, task view, Explorer |
| Apps | Notepad, Calculator, Browser, Terminal, Steam, Discord |
| Power | Shut down, restart, sleep, hibernate, sign out, lock |
| Type | Live keyboard, copy-save from PC, typed shortcuts |
| Mouse | Drag pad + left/right click |

## SD card

Copy the examples from `sd_template/` to the **root** of a FAT32 card, or let the firmware create empty files:

| File                 | Role |
|----------------------|------|
| `/data.json`         | Wallet entries |
| `/shortcuts.json`    | Type-category text shortcuts |
| `/helper-path.txt`   | Optional. One-line path to `clip-helper.ps1` |

`data.json` holds real passwords in plain text. Do not commit a filled-in copy.

## Security

- The wallet unlock password is hashed in ESP32 NVS.
- Usernames and passwords on the SD card are **not encrypted**. Anyone with the card can read them.
- Treat this as a convenience device, not a hardware security key.

## Project layout

```
arduino/WalletRemote/   Arduino sketch (WalletRemote.ino + firmware .cpp)
platformio.ini          PlatformIO build (src_dir points at the sketch)
tools/                  Bluetooth clipboard helper (Windows)
scripts/                rebuild and flash
sd_template/            example SD files
docs/                   tutorial and menu pictures
dist/                   merged flash image after a build
```

## Board variant

Most USB-C “7789” units work with the current `ST7789_DRIVER` flags in `platformio.ini`. Single micro-USB ILI9341 boards: comment the ST7789 lines and uncomment `ILI9341_DRIVER`.

- **Black screen** — flash `dist/wallet-remote.bin` at `0x0`, not `firmware.bin`
- **Garbled / inverted colors** — toggle `TFT_INVERSION_ON`
- **Touch mirrored** — flip `TOUCH_INVERT_X`, `TOUCH_INVERT_Y`, or `TOUCH_SWAP_XY` in `arduino/WalletRemote/config.h`

## Build from source

Arduino sketch: **[`arduino/WalletRemote/WalletRemote.ino`](arduino/WalletRemote/WalletRemote.ino)**  
Arduino IDE steps: **[docs/ARDUINO_IDE.md](docs/ARDUINO_IDE.md)**

[PlatformIO](https://platformio.org/) + Python 3 (recommended):

```powershell
python -m platformio run -e cyd
powershell -ExecutionPolicy Bypass -File scripts\rebuild.ps1
```

## License

MIT — see [LICENSE](LICENSE).
