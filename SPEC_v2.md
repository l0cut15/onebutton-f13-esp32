# Spec v2: OneButton F13 with a config drive (ESP32-S2 mini)

Status: implemented and tested on hardware (2026-10-28): all acceptance tests in section 10 pass. Section 12 records what was checked in the esp_tinyusb 2.4.0 source and on the device. Builds on [SPEC.md](SPEC.md) (v1). Everything in v1 still applies unless this file changes it. v1 section 9 lists runtime configuration as out of scope. v2 adds it.

## 1. Goal

Make the key chord configurable without rebuilding the firmware. In normal use the device is exactly the v1 HID keyboard. Pressing the board's BOOT button restarts it as a small USB drive holding a text file, `CONFIG.TXT`. The user edits the file, ejects the drive, and the device restarts as a keyboard that sends the new chord.

No host software and no drivers. macOS is the tested host. The drive is a standard FAT volume, so it should work on Windows and Linux too.

## 2. Modes

| Mode | USB interfaces | Entered by | Left by |
|---|---|---|---|
| `NORMAL` | HID keyboard only (v1 descriptors, VID/PID unchanged) | any power-up, any reset without the config flag | holding BOOT for `PTT_CONFIG_HOLD_MS` |
| `CONFIG` | Mass storage only (one LUN, FAT) | restart with the config flag set | eject from the host, or holding BOOT again, or unplugging |

- The mode is chosen once at boot. Switching modes is always a restart (`esp_restart()`). Changing USB descriptors while the device is running is not attempted.
- The config flag is a magic word in `RTC_NOINIT_ATTR` memory. Set it, then restart. At boot, read it and clear it straight away, so one flag gives exactly one config-mode boot. Power loss clears RTC memory, so unplugging always comes back in `NORMAL`.
- `CONFIG` mode enumerates with a separate PID (`PTT_USB_PID_CONFIG`, default 0x4002). Same VID, manufacturer and serial, product string `"<PTT_USB_PRODUCT> Config"`. Hosts cache interface sets per VID/PID, so the drive should not reuse the keyboard's PID.
- In `CONFIG` mode the main key does nothing. HID is not enumerated, so no key can get stuck.

## 3. Hardware

| Item | Default | Notes |
|---|---|---|
| Main key | GPIO17 | v1, unchanged |
| Config button | GPIO0 (onboard BOOT) | New Kconfig `PTT_CONFIG_BUTTON_GPIO`, active low, board pull-up plus the internal pull-up. -1 disables config mode entirely |
| Status LED | GPIO15 | v1. Adds the patterns in section 7 |

v1 section 3 forbids GPIO0. v2 changes that rule: GPIO0 may be used **only as the config button**, read as an input after boot. Its strapping role does not change. Holding BOOT while the board powers up still enters the ROM download mode, and that stays the recovery path. Keep the compile-time rejection of GPIO0 for `PTT_BUTTON_GPIO`.

The case already has a BOOT window in the -X wall (case spec section 3), so the button can be reached without opening the case.

## 4. Storage

### 4.1 Partition table

A custom `partitions.csv` (set `CONFIG_PARTITION_TABLE_CUSTOM=y` in `sdkconfig.defaults`). 4 MB flash:

| Name | Type | SubType | Offset | Size |
|---|---|---|---|---|
| nvs | data | nvs | 0x9000 | 0x6000 |
| phy_init | data | phy | 0xF000 | 0x1000 |
| factory | app | factory | 0x10000 | 0x100000 (1 MB) |
| storage | data | fat | 0x110000 | 0x100000 (1 MB) |

The remaining ~1.9 MB is left free on purpose. That leaves room for a later OTA layout.

### 4.2 Filesystem

- Wear-levelled FAT on `storage`, through `wear_levelling` and esp_tinyusb's `tinyusb_msc_new_storage_spiflash()` (esp_tinyusb 2.4.0, `tinyusb_msc.h`).
- Volume label `ONEBUTTON`. File names in 8.3 upper case (`CONFIG.TXT`, `STATUS.TXT`) so they work even with long file names disabled.
- Only one side owns the volume at a time. In `NORMAL` mode the firmware mounts it locally (`TINYUSB_MSC_STORAGE_MOUNT_APP`, or plain `esp_vfs_fat_spiflash_mount_rw_wl()` without MSC installed), reads and writes what it needs, then unmounts. In `CONFIG` mode it is exposed only to the host (`TINYUSB_MSC_STORAGE_MOUNT_USB`). The firmware never touches the volume while the host has it.
- 1 MB is far more than the two files need. The headroom is for the hidden files macOS writes (`.fseventsd`, `._*`, `.Spotlight-V100`) so they never fill the volume.

### 4.3 First boot and damaged volumes

At every `NORMAL` boot:
1. Mount the FAT. If the mount fails, format the partition and mount again. Write `STATUS.TXT` noting that storage was reformatted.
2. If `CONFIG.TXT` is missing, write the default file (section 5.3) built from `ptt_config.h`.
3. Read and parse `CONFIG.TXT` (section 5), then write `STATUS.TXT` (section 6) **only if its contents changed**, to save flash wear.
4. Unmount, then start USB as HID.

This makes a deleted `CONFIG.TXT` a factory reset, and a corrupted volume (unplugged without ejecting) repairs itself.

## 5. CONFIG.TXT

### 5.1 Format

Plain text, UTF-8 or ASCII. LF or CRLF line endings. Read at most 4 KB, and treat anything longer as an error.

```
# OneButton F13 configuration
# Edit, save, then eject the ONEBUTTON drive to apply.

chord = F13
```

- One `name = value` per line. Whitespace around the name and value is ignored. Names are case-insensitive.
- `#` starts a comment, anywhere on a line. Blank lines are ignored.
- Unknown names produce a warning in `STATUS.TXT` and are otherwise ignored, so newer files still load on older firmware.

### 5.2 Settings

| Name | Required | Value | Default |
|---|---|---|---|
| `chord` | yes | Modifiers and keys joined by `+`, e.g. `ctrl+shift+F13`, `alt+space`, `F15` | from `ptt_config.h` (`F13`) |
| `max_hold_s` | no | 0 to 3600, 0 = unlimited | `PTT_MAX_HOLD_S` |

Timing settings (`PTT_DEBOUNCE_MS` and the two modifier/key delays) stay in Kconfig. They only matter when tuning for a particular app and are not worth exposing.

**Chord grammar**: case-insensitive tokens separated by `+`, with optional spaces around each `+`. Modifiers may appear in any order. At least one key or modifier, and at most 6 non-modifier keys. A chord that is only modifiers (e.g. `ctrl`) is allowed, matching v1.

| Modifier token | HID bit |
|---|---|
| `ctrl`, `lctrl` / `rctrl` | Left / Right Control |
| `shift`, `lshift` / `rshift` | Left / Right Shift |
| `alt`, `option`, `lalt` / `ralt` | Left / Right Alt |
| `cmd`, `gui`, `win`, `lcmd` / `rcmd` | Left / Right GUI |

The plain forms (`ctrl`, `shift`, `alt`, `cmd`) mean the left-hand key.

Key tokens (HID usage page 0x07):
- `A`–`Z`, `0`–`9`, `F1`–`F24`
- `space`, `enter` (`return`), `tab`, `esc` (`escape`), `backspace`, `delete`, `insert`, `home`, `end`, `pageup`, `pagedown`, `up`, `down`, `left`, `right`
- `grave`, `minus`, `equal`, `lbracket`, `rbracket`, `backslash`, `semicolon`, `quote`, `comma`, `period`, `slash`
- `capslock`, `printscreen`, `scrolllock`, `pause`, `menu`, `mute`, `volumeup`, `volumedown`
- A raw usage ID in hex, `0x04`–`0xDF`, for anything not in the table. 0xE0–0xE7 are rejected: use the modifier tokens instead.

The name table lives in its own file (`main/ptt_keynames.c`) and is the only place key names are defined. The default file and `STATUS.TXT` both use it to print chords back.

### 5.3 Default file

Written when `CONFIG.TXT` is missing. The `chord` line is generated from `PTT_MODIFIERS` and `PTT_KEYS`. The comments list the supported modifier names and point to the README for the full key list. The default file must parse with no warnings (acceptance test 3).

### 5.4 Errors

If `chord` is missing or invalid, or the file is unreadable, the device uses the **factory default** chord from `ptt_config.h`. It does not keep the previous chord. That is predictable, and it needs no second copy of the settings in NVS. An invalid optional setting falls back to its own default, while the rest of the file still applies. Every error and warning goes to `STATUS.TXT` with its line number, and the boot LED pattern (section 7) shows that an error happened.

## 6. STATUS.TXT

Written by the firmware in `NORMAL` mode, read by the user in `CONFIG` mode. The device overwrites it, and any edits to it are ignored.

```
OneButton F13 status
firmware: 2.0.0
config: OK
chord: ctrl+F13
max_hold_s: 120
```
or
```
config: ERROR, using factory default
line 4: unknown key "F31"
chord: F13
```

## 7. Behaviour

### 7.1 Entering config mode
- Debounce the config button the same way as the main key. A hold of `PTT_CONFIG_HOLD_MS` (default 1000) triggers it. That is long enough not to fire by accident and short enough to need no instructions.
- If the main chord is currently held, release it first (v1 release sequence, ending in an all-zero report). Then set the flag and restart.
- Holding BOOT across power-up enters ROM download mode, not config mode. This is expected, and the README should say so.

### 7.2 In config mode
- Expose the volume to the host as MSC.
- **Eject**: on SCSI START STOP UNIT with `LoEj = 1, Start = 0`, wait about 200 ms for the status phase to finish, then restart into `NORMAL`. esp_tinyusb owns `tud_msc_start_stop_cb`, so the eject is seen through its storage event callback (section 12).
- **BOOT held again** for `PTT_CONFIG_HOLD_MS`: restart into `NORMAL`. The README must say to eject first. Otherwise macOS may not have written the file yet, and it shows a "Disk Not Ejected Properly" warning.
- **Unplug**: next power-up is `NORMAL`. Same caveat about unwritten data.
- There is no automatic timeout. A timeout could restart while the user is still editing.

### 7.3 LED
| Pattern | Meaning |
|---|---|
| on while the key is held | v1, unchanged |
| 3 short blinks at boot | `NORMAL` mode, `CONFIG.TXT` had an error, factory chord in use |
| slow blink (1 s period) | `CONFIG` mode |

## 8. Code layout

v1 files stay. New and changed files:

| File | Purpose |
|---|---|
| `main/main.c` | Read and clear the RTC flag, pick the mode, call the storage setup, then start HID or MSC |
| `main/ptt_mode.c/h` | RTC flag, `ptt_mode_enter_config()`, `ptt_mode_enter_normal()` |
| `main/ptt_storage.c/h` | Mount, format, unmount, read/write `CONFIG.TXT` and `STATUS.TXT` |
| `main/ptt_cfgfile.c/h` | Parser: text in, `ptt_settings_t` and a list of diagnostics out. No I/O, so it can be unit-tested on the host |
| `main/ptt_keynames.c/h` | Name ↔ usage ID table, chord formatting |
| `main/ptt_msc.c/h` | Config-mode USB: MSC descriptors, eject callback |
| `main/ptt_hid.c` | Takes the chord from `ptt_settings_t` at runtime, not from the `ptt_config.h` macros |
| `main/ptt_button.c` | Second button instance for GPIO0, with hold-time detection |
| `main/ptt_config.h` | Kept. Now means "factory default chord". The `static_assert` checks stay |
| `main/Kconfig.projbuild` | Adds `PTT_CONFIG_BUTTON_GPIO`, `PTT_CONFIG_HOLD_MS`, `PTT_USB_PID_CONFIG` |
| `partitions.csv` | Section 4.1 |
| `sdkconfig.defaults` | `CONFIG_PARTITION_TABLE_CUSTOM=y`, `CONFIG_TINYUSB_MSC_ENABLED=y`, FAT LFN optional |
| `test/` | Host build of `ptt_cfgfile.c` and `ptt_keynames.c` with a table of good and bad inputs |

## 9. Firmware image and flashing

- The prebuilt image stays a single merged `.bin` written at 0x0. It contains the bootloader, the new partition table and the app. The `storage` partition is not in the image. On first boot it is blank, so it gets formatted and the default `CONFIG.TXT` is written (section 4.3).
- Reflashing the app with `write_flash 0x0` leaves `storage` in place, so settings survive firmware updates. `esptool.py erase_flash` wipes them.
- Upgrading from v1 needs nothing special, because the new partition table goes in with the image. The README flash command stays the same.

## 10. Acceptance tests

1. Fresh `erase_flash` + flash: the device enumerates as the v1 keyboard and sends F13. In `CONFIG` mode the drive shows `CONFIG.TXT` with the default chord, and `STATUS.TXT` with `config: OK`.
2. Change `chord = ctrl+shift+F15`, save, eject: within 3 s the device re-enumerates as a keyboard. Karabiner-EventViewer shows the modifiers down, then F15 down, then the reverse on release (v1 timing).
3. The default file parses with no warnings.
4. Bad files, one at a time: unknown key, 7 keys, `0xE0`, empty file, deleted file, 10 KB file, binary junk. Each one gives the factory chord, a correct `STATUS.TXT` line and the 3-blink LED. A deleted file gets regenerated.
5. Unplug in `CONFIG` mode without ejecting, right after saving. On replug the device boots `NORMAL`. If FAT mounting fails, it reformats and reports that in `STATUS.TXT`.
6. Hold BOOT while the main key is held: the chord is released (no stuck key on the host) before the device restarts into `CONFIG`.
7. Tap BOOT briefly (under 1 s): nothing happens.
8. Plug in while holding BOOT: ROM download mode (serial port appears), as in v1.
9. Settings survive reflashing the prebuilt image over an existing device.
10. Host unit tests for the parser and the key table pass.

## 11. Out of scope

Live config without a restart, HID and MSC together in one composite device, a web or WebUSB config page, multiple chords or profiles, macros, OTA.

## 12. Open items: findings

Checked against the esp_tinyusb 2.4.0 and TinyUSB 0.21.0~2 sources (`managed_components/`) and ESP-IDF 5.5.4.

**Eject callback.** esp_tinyusb defines `tud_msc_start_stop_cb` as a strong symbol in `tinyusb_msc.c` (TinyUSB's is weak), so the firmware cannot define its own: it would not link. On `LoEj = 1, Start = 0` it calls `msc_storage_mount_to_app()`, which mounts the volume for the app and fires the storage event callback with `TINYUSB_MSC_EVENT_MOUNT_COMPLETE` and `mount_point == TINYUSB_MSC_STORAGE_MOUNT_APP` (or `MOUNT_FAILED` / `FORMAT_REQUIRED` if the FAT does not mount). `ptt_msc.c` treats any of these as the eject. The callback runs in the TinyUSB task inside the SCSI command, before the status phase, so it only posts an event; the config task waits 200 ms and restarts. After the hand-back, `tud_msc_test_unit_ready_cb` reports "medium not present", so the host sees the drive gone. TinyUSB reports the LUN as removable (`is_removable = 1` in the INQUIRY response), which is what makes a host send START STOP UNIT on eject. **Confirmed on hardware:** a Finder eject sends it. The event arrives as `MOUNT_COMPLETE` (APP), and the device is back as the keyboard 0.94 s after the volume unmounts. Ejecting a volume whose FAT was zeroed arrives as `FORMAT_REQUIRED` and also restarts. Holding BOOT stays as a fallback.

**Auto-remount.** `auto_mount_off` must stay 0: with it set, `msc_storage_mount_to_app()` does nothing on eject and no event fires. The same hand-back also runs from `tud_umount_cb` (host deconfigures or the bus detaches), which then also restarts into `NORMAL`; that is the right outcome. No race with the restart: the remount completes synchronously in the TinyUSB task before the event is posted, and it only reads the volume, provided it cannot format. So `CONFIG` mode creates the storage with `fat_fs.do_not_format = true`. Without that, a FAT the host left unmountable would be reformatted on eject, losing the user's file. Host writes are deferred to the TinyUSB task (`usbd_defer_func`) but queued ahead of the next SCSI command, so they are on flash before START STOP UNIT is handled.

**Restart.** On the ESP32-S2, `esp_restart()` ends in `esp_rom_software_reset_cpu()`. That is a CPU reset: strapping pins are not re-sampled, so restarting while BOOT is still held does not enter download mode. It also does not reset the USB peripheral (`esp_system_reset_modules_on_exit()` skips it), so `ptt_mode.c` calls `tud_disconnect()` and waits 100 ms before restarting, so the host sees the device leave. The config flag is only honoured when `esp_reset_reason() == ESP_RST_SW`, which covers random RTC contents after power-on and panics.

**FAT size.** `CONFIG_WL_SECTOR_SIZE` is 4096 by default, and `tinyusb_msc_new_storage_spiflash()` refuses to start unless `CONFIG_TINYUSB_MSC_BUFSIZE` (512 by default on the S2) is at least that size, so `sdkconfig.defaults` sets it to 4096. The host sees 4096-byte blocks. Wear levelling on 1 MB (256 sectors) keeps 2 × 2 state sectors (256 position records of 16 bytes do not fit one sector), 1 config sector and 1 spare sector, which leaves 250 sectors (1000 KB). The IDF formatter makes FAT12 with 4 KB clusters, 1 reserved sector, 2 FATs of 1 sector each and FatFs's default 512 root entries (4 sectors; IDF only shrinks that below 128 sectors), so about 243 clusters (972 KB) are free. **On hardware:** macOS mounts it as MS-DOS FAT12 with 4096-byte blocks, 999 KB total and 979 KB free.

**Volume label.** `f_setlabel()` needs `CONFIG_FATFS_USE_LABEL=y` (set in `sdkconfig.defaults`). The label is written only when the firmware formats the volume, so a user rename survives.

**Formatting in NORMAL mode** uses `esp_vfs_fat_spiflash_mount_rw_wl()` without MSC installed. A failed FAT mount leaves the wear-levelling layer mounted, so the firmware unmounts it before mounting again with `format_if_mount_failed`. If that also fails, it erases the partition and tries once more.

**Keyboard Setup Assistant** after leaving config mode: does not appear (checked on hardware). The keyboard's VID/PID/serial are the same as before, and the drive uses its own PID.

**Deviations from the text above**, all small:
- A deleted `CONFIG.TXT` is regenerated and parses cleanly, so it does not trigger the 3-blink pattern. `STATUS.TXT` says `CONFIG.TXT: was missing, default written`. Acceptance test 4 lists the deleted file among the cases that blink; section 7.3 defines the blink as "had an error". This follows 7.3.
- An invalid `max_hold_s` is a warning, not an error: it falls back to its default and does not blink, since the chord still applies.
- A line without `=` and a duplicate setting are warnings (first one wins). A file with control characters (binary junk) is an error.
- `PTT_USB_PRODUCT` is limited to 24 characters, so that `"<product> Config"` fits esp_tinyusb's 31-character string limit.

## 13. Implementation status

Implemented in commit f899cd7 and tested on a Lolin S2 mini with macOS on 2026-10-28.

| # | Test | Result |
|---|---|---|
| 1 | Fresh flash: keyboard, then drive with defaults | Pass |
| 2 | `ctrl+shift+F15`, eject | Pass: keyboard again 0.94 s after the eject, modifiers down before F15, released after it |
| 3 | Default file parses with no warnings | Pass (host test and device) |
| 4 | Bad files | Pass on device for unknown key, 10 KB file, binary junk (3 blinks, factory chord, correct `STATUS.TXT` line) and deleted file (regenerated, no blink). Empty file, 7 keys and `0xE0` take the same path and pass in the host tests |
| 5 | Unplug in `CONFIG` mode right after saving | Pass: the save survived. A volume with its first 64 KB zeroed was reformatted, and `STATUS.TXT` said so |
| 6 | Hold BOOT while the key is held | Pass: chord released before the restart |
| 7 | Tap BOOT briefly | Pass: nothing happens |
| 8 | Plug in holding BOOT | Pass: ROM download mode, serial port appears |
| 9 | Settings survive reflashing the prebuilt image at 0x0 | Pass |
| 10 | Host unit tests (`make -C test`) | Pass, 658 checks |

Notes from testing:
- The first test board had a faulty BOOT button. Its symptoms (restarts that came back as the keyboard) looked like a firmware fault until the board was swapped.
- In one deleted-file run, a second keyboard boot happened between the eject and entering config mode, so `STATUS.TXT` was rewritten without the "was missing" note. A repeat run with every boot logged showed a single software restart and the correct note. The cause of the extra boot is unknown; it did not recur.
- After flashing, esptool's watchdog reset brought the board up once on its own and once only after tapping RST. The README already says to tap RST.
- The prebuilt image also flashes with the Espressif web flasher (esptool-js) at 0x0, and settings survive it.
- Not yet tested: Windows and Linux hosts.
