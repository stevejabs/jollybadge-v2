# JollyBadge V2

JollyBadge V2 is a sequential puzzle / challenge badge. It will test your observational skills, require a bit of software tooling, some OSINT, and—if you want to go really deep—even a splash of hardware hacking. This document walks you through updating the badge firmware safely on macOS, Linux, and Windows.

---

## Firmware releases

Pre-built firmware images live in the `builds/` directory.

* **Latest release candidate:** `JollyBadgeV2-2.0.0.bin`
* Earlier versions exist if you ever want to go back and try those challenges
* 2.X.Y src will not be released until later this year

---

## Requirements

1. **BOSSA / bossac CLI 1.9 or newer**  
   * Project page: <https://github.com/shumatech/BOSSA>  
   * macOS: `brew install bossac` (or download the macOS release dmg)  
   * Windows: download the official installer – it places `bossac.exe` in your *Program Files/BOSSA* folder  
   * Linux: most distros ship a `bossa` or `bossac` package, or build from source.
2. A **USB-C data cable** (charge-only cables will not work).

---

## Flashing the badge step-by-step

> The procedure is identical on every OS – only the serial-port name and, occasionally, the `bossac` executable name differ.

1. **Connect the badge** to your computer via USB-C.
2. **Enter bootloader mode**: double-press the RESET button – the LEDs will freeze, indicating the bootloader is active. The reset button is accessible without disassembling JollyBadge via the hole on the back enclosure.
3. **Find your serial/COM port** (instructions below).
4. **Close *all* serial monitors or terminal programs** that might have the port open (Arduino IDE Serial Monitor, screen, minicom, CoolTerm, etc.). bossac needs exclusive access.
5. **Run the bossac command** for your OS, replacing the port name you found in step 3 **and the firmware filename if necessary**.

### 3 · How to identify the correct port

macOS
```
ls /dev/tty.usbmodem*
# Example output: /dev/tty.usbmodem1301
```
Use the part after `/dev/` (e.g. `tty.usbmodem1301`) in the `--port=` argument.

Linux
```
dmesg | grep -i tty
ls /dev/ttyACM*
# Usually /dev/ttyACM0 or similar
```
Pass the *full* device name to `--port=` (e.g. `--port=/dev/ttyACM0`).

Windows
1. Open **Device Manager** → **Ports (COM & LPT)**.  
2. Look for **“Bossa Program Port (COMx)”** – note the `COM` number.

On PowerShell or cmd just use `COM7` (replace with your number). If you hit path issues, prefix with `//./` (rare).

### 5 · bossac commands per OS

macOS / Linux
```
bossac -i -d --port=tty.usbmodem1301 --offset=0x2000 -e -w -v -R JollyBadgeV2-2.0.0.bin
```

Windows (PowerShell / cmd)
```
"C:\Program Files\BOSSA\bossac.exe" -i -d --port=COM7 --offset=0x2000 -e -w -v -R JollyBadgeV2-2.0.0.bin
```
*If bossac is in your `PATH`, you can omit the absolute path and `.exe` extension.*

## Why `--offset=0x2000` **must not change**
The first **8 KiB** of flash (addresses `0x0000–0x1FFF`) contain the UF2 bootloader that lets you recover the badge if something goes wrong. Flashing at offset **`0x0000` will overwrite the bootloader and brick the device** – you’d need an external SWD programmer to fix it. **Leave the offset exactly as shown.**