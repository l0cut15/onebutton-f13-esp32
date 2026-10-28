# Spec: One-Button USB HID Push-to-Talk Controller (ESP32-S2 mini)

## 1. Goal

A single physical button on an ESP32-S2 mini (Wemos/Lolin S2 mini) that enumerates over native USB as a HID keyboard. While the button is held, the device holds a configurable key chord (default: Left Ctrl + ` [grave]). On release, it releases the chord. The macOS host dictation app starts recording on chord-down and processes the dictation on chord-up.

This is **hold semantics, not tap**. The chord must stay asserted for the full duration of the button hold.

## 2. Targets and constraints

- Board: ESP32-S2 mini, native USB (D+ GPIO20, D- GPIO19). No USB-Serial-JTAG exists on the S2.
- Toolchain: **ESP-IDF 5.5.4**, C, CMake. (PlatformIO is out of scope unless trivially portable.)
- USB stack: TinyUSB via the managed component `espressif/esp_tinyusb`. **Pin an exact version in `idf_component.yml`** and verify every API call against that version's docs and its bundled `device/hid` example, because the esp_tinyusb API changed between 1.x and 2.x. Do not guess signatures.
- Host: macOS. Must work with no driver and no host-side software.
- Build-time configuration only. No runtime config, no CDC/console over USB.
- Open-source only. Personal use, so Espressif's test VID/PID (0x303A / 0x4001) is acceptable; make VID/PID/strings configurable in Kconfig.

## 3. Hardware

| Item | Default | Notes |
|---|---|---|
| Button GPIO | GPIO17 | Kconfig `PTT_BUTTON_GPIO` |
| Wiring | GPIO to GND, internal pull-up | Active low, Kconfig `PTT_BUTTON_ACTIVE_LOW` |
| Status LED | GPIO15 (onboard) | Kconfig `PTT_LED_GPIO`, set -1 to disable |
| Do not use | GPIO19, GPIO20 (USB), GPIO0 (boot strap) | Enforce with a Kconfig range or `static_assert` |

## 4. Configuration

### 4.1 Chord: `main/ptt_config.h` (edited by the user, rebuild to apply)

```c
// Modifier bitmask: use TinyUSB KEYBOARD_MODIFIER_* constants
#define PTT_MODIFIERS   (KEYBOARD_MODIFIER_LEFTCTRL)
// Up to 6 non-modifier HID usage IDs, zero-terminated
#define PTT_KEYS        { HID_KEY_GRAVE, 0, 0, 0, 0, 0 }
```

Compile-time validation: at most 6 keys, no modifier usage IDs (0xE0 to 0xE7) in `PTT_KEYS`, at least one modifier or key set. Fail the build with `static_assert` and a readable message otherwise.

### 4.2 Timing and behavior: Kconfig (`menuconfig` under "PTT Controller")

| Symbol | Default | Meaning |
|---|---|---|
| `PTT_DEBOUNCE_MS` | 20 | Stable-state time before accepting an edge |
| `PTT_MOD_TO_KEY_DELAY_MS` | 10 | On press: send modifier-only report, wait, then send modifier+key report. Mimics a human chord so apps that watch modifier-then-key ordering behave |
| `PTT_KEY_TO_MOD_DELAY_MS` | 10 | On release: send modifier-only report (key up), wait, then send empty report |
| `PTT_MAX_HOLD_S` | 120 | Safety: force-release if held longer than this. 0 disables |
| `PTT_REMOTE_WAKEUP` | n | If y, a press while the host is suspended calls `tud_remote_wakeup()` |
| `PTT_USB_VID/PID/MANUFACTURER/PRODUCT` | 0x303A / 0x4001 / "DIY" / "PTT Button" | Descriptor strings |

## 5. Behavior (state machine)

States: `IDLE`, `HELD`.

- `IDLE` to `HELD` (debounced press, and `tud_mounted()` and not suspended, or remote wakeup enabled and used):
  1. Send report: modifiers = `PTT_MODIFIERS`, keys = none.
  2. Delay `PTT_MOD_TO_KEY_DELAY_MS`.
  3. Send report: modifiers + `PTT_KEYS`.
  4. LED on.
- `HELD` to `IDLE` (debounced release, **or** USB unmounted/suspended, **or** max-hold timeout):
  1. Send report: modifiers only (keys up).
  2. Delay `PTT_KEY_TO_MOD_DELAY_MS`.
  3. Send all-zero report.
  4. LED off.
- Button pressed while USB is not mounted: ignore, do not queue. If the button is still held when USB mounts, do not send anything until the next press (prevents a spurious chord on plug-in).
- Never leave a key stuck. Every exit from `HELD` must end with an all-zero report, and the all-zero report is also sent once on every `tud_mount_cb`.
- HID report sends must wait for `tud_hid_ready()` (with a short bounded retry), never block forever, and never be called from an ISR.

## 6. Implementation requirements

- GPIO: interrupt on any edge posts to a FreeRTOS queue; a single task does debounce (re-read the pin after `PTT_DEBOUNCE_MS`, accept only if stable) and drives the state machine. Alternative: 1 ms polling task. Either is fine, but no logic in the ISR beyond `xQueueSendFromISR`.
- USB: install via esp_tinyusb with a custom device descriptor (VID/PID/strings from Kconfig), one HID interface using `TUD_HID_REPORT_DESC_KEYBOARD()` (boot-compatible 8-byte report, no report ID is simplest; if a report ID is used, use it consistently in descriptor and send call). Implement the required HID callbacks (`tud_hid_descriptor_report_cb`, `tud_hid_get_report_cb`, `tud_hid_set_report_cb`). Ignore LED output reports.
- HID descriptor `bCountryCode`: set to 33 (US) in the HID class descriptor if the TinyUSB config allows. Goal is to avoid the macOS "Keyboard Setup Assistant" prompt on first plug-in. Treat this as a try-and-verify item: if it does not suppress the prompt, document that the user dismisses it once (macOS keys it to VID/PID/serial).
- Logging: no USB console. Default `CONFIG_ESP_CONSOLE_NONE` or UART0 (TX GPIO43) for debug builds only, selectable in Kconfig. Production build has logging at WARN or below.
- Power: no sleep modes. Keep it simple and always-on.
- Code layout: `main/main.c` (init), `main/ptt_button.c/h` (GPIO + debounce), `main/ptt_hid.c/h` (USB + report sending), `main/ptt_config.h`, `main/Kconfig.projbuild`, `main/idf_component.yml`, `sdkconfig.defaults` (target esp32s2, TinyUSB enabled, console none), `README.md`.

## 7. Deliverables

1. Buildable project: `idf.py set-target esp32s2 && idf.py build` succeeds with zero warnings on IDF 5.5.4.
2. `README.md` with exact commands for build, flash, and recovery, including:
   - The S2 has no USB-Serial-JTAG. Once this firmware owns USB, `idf.py flash` cannot auto-reset the chip. To reflash: hold the **0** button, tap **RST**, release **0** (ROM bootloader enumerates as a serial port), then `idf.py -p /dev/cu.usbmodem* flash`, then tap RST.
   - How to change the chord in `ptt_config.h` with a table of useful HID usage IDs (grave 0x35, F13 to F24 0x68 to 0x73, space 0x2C).
   - How to bind the dictation app to the chord, and the note that F13 to F24 are collision-free alternatives.
3. Wiring diagram in ASCII in the README.

## 8. Acceptance tests (agent must run what it can, document what it cannot)

Host-side checks on macOS after flashing:

1. `system_profiler SPUSBDataType | grep -A8 "PTT Button"` shows the device with the configured VID/PID and strings.
2. Open Karabiner-EventViewer, or any browser key-tester page. Press and hold the button: Left Ctrl down, then grave down, ordered, with about 10 ms between them. Release: grave up, then Ctrl up. No repeat events beyond the host's normal key repeat for the grave key.
3. Hold 130 s (with default `PTT_MAX_HOLD_S=120`): chord auto-releases at 120 s.
4. Unplug USB while held, replug: no stuck modifier on the host, no chord sent on replug while the button is still held.
5. Bounce test: tap the button very quickly 20 times: every press produces exactly one down/up pair, no orphaned keys.
6. Optional if enabled: put the Mac to sleep, press the button, the Mac wakes.

## 9. Out of scope

Runtime configuration, BLE, multiple buttons, macros with typed text, Windows/Linux-specific tuning (should work unchanged, but only macOS is tested), OTA updates.

## 10. Open items for the agent to flag, not guess

- Exact esp_tinyusb version and API for descriptor override and HID country code.
- Whether the ESP32-S2 USB OTG peripheral reports suspend and resume reliably enough for the remote-wakeup path. If not, document the limitation.
