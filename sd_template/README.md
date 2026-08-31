# SD card files

Format the microSD card as **FAT32**, then copy these files to the **root**:

| File | Purpose |
|------|---------|
| `data.json` | Password wallet entries |
| `shortcuts.json` | Macro Pad Type shortcuts |
| `helper-path.txt` | Optional. Full path to `clip-helper.ps1` |

Start from the example files:

```
copy data.example.json         <SD>:\data.json
copy shortcuts.example.json    <SD>:\shortcuts.json
copy helper-path.example.txt   <SD>:\helper-path.txt
```

You can also leave the card empty. The firmware creates `data.json` and `shortcuts.json` on first use. Add, pin, and delete entries from the touchscreen.

`data.json` holds real passwords in plain text. Do not commit a filled-in copy to git.
