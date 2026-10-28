# One-Button USB HID Push-to-Talk (ESP32-S2 mini)

A single button on a Lolin/Wemos S2 mini that enumerates over native USB as a HID keyboard.
While the button is **held**, the device holds a key chord (default **Left Ctrl + `**). On release, it
releases the chord. No driver and no host software are needed.

- ESP-IDF **5.5.4**, C
- `espressif/esp_tinyusb` **==2.4.0** (pinned in `main/idf_component.yml`; pulls `espressif/tinyusb` 0.21.0~2)
- Full design: [SPEC.md](SPEC.md)
- 3D printed case: [_Case/](_Case/) (see [Case](#case))

## Wiring

```
             Lolin S2 mini
         +-------------------+
         |  [0]       [RST]  |   <- onboard buttons (used for recovery)
         |                   |
         |  GPIO17 o---------+------.
         |                   |      |
         |                   |    --+--   momentary push button
         |                   |    --+--   (normally open)
         |                   |      |
         |  GND    o---------+------'
         |                   |
         |  GPIO15 = onboard LED (lit while chord is held)
         |                   |
         +------[USB-C]------+
                   |
                to the Mac (GPIO19 D-, GPIO20 D+, native USB)
```

The internal pull-up is used, so the button is active low and needs no resistor.
Do not use GPIO0 (boot strap), GPIO19/20 (USB) or GPIO26–32 (in-package flash/PSRAM). The build
rejects them with a `static_assert`.

## Case

A four-part, screwless, 3D printed case for the S2 mini and a Kailh Box Navy (MX compatible) switch
lives in [_Case/](_Case/). It is about 30 x 39 x 30 mm with the keycap fitted.
[_Case/OneButton_design_spec.md](_Case/OneButton_design_spec.md) is the full spec: every dimension,
fit, print orientation and open item, with enough detail to rebuild the model in any CAD tool.

| Part | File | What it does |
|---|---|---|
| Case shell | `OneButton_CASE_SHELL.stl` | Body with the switch seat, board posts and pegs, USB-C opening, BOOT window and RST hole |
| Bottom cap | `OneButton_BOTTOM_CAP.stl` | Floor with board bosses and four snap arms that latch into windows in the shell |
| Keycap | `OneButton_KEYCAP.stl` | 24.3 mm square cap with an MX cross socket |
| Shroud | `OneButton_SHROUD.stl` | Tapered bezel whose bore guides the keycap so it does not rock. Glued to the shell with four pegs |

- **Hardware**: the board sits components-down under the switch. The switch is wired to **GPIO17**
  and **GND** with two short flexible wires (about 15 mm plus slack), soldered to the switch pins and
  board pads before the board goes in.
- **Flashing**: BOOT (0) and RST stay reachable through a window and a hole in the side walls, so the
  case does not have to be opened to reflash.
- **Printing**: PLA, 0.4 mm nozzle, 0.2 mm layers, 3+ perimeters, 15–20% infill, no supports. The STLs
  are exported in design orientation (Y-up from Fusion), so lay them flat in the slicer: shell top face
  down, cap floor down, keycap plate down, shroud top face down. Print order: keycap, bottom cap, shell,
  shroud.
- **Assembly**: solder the wires, seat the switch in the shell, drop the board onto the pegs, snap the
  bottom cap in, glue the shroud on, fit the keycap. To open, press a hook in through its latch window.
- **Rebuilding the model**: `_Case/onebutton_build.py` is a Fusion 360 script that builds all four
  bodies (plus reference bodies for the board and switch) from the parameters at its top, runs an
  interference check, and can export the STLs (`EXPORT_DIR`). Run it in an empty parametric design.
  Change `LOG_PATH` first: it points at a local path.
- **Status**: prototype (spec v0.11). Unverified on real parts: switch travel against the keycap
  sleeve, peg and keycap fits, snap-arm strength in PLA. See section 7 of the spec.

## Build

```bash
. ~/esp/esp-idf-v5.5.4/export.sh
idf.py set-target esp32s2      # once; also applies sdkconfig.defaults
idf.py build
```

`sdkconfig` is generated from `sdkconfig.defaults` and is not committed. Delete it to pick up changes
to `sdkconfig.defaults`.

### Debug build (UART log on GPIO43)

The production build has no console and logs at WARN. For logging, build into a separate directory
with the debug overlay:

```bash
idf.py -B build-debug -D SDKCONFIG=sdkconfig.dbg \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.debug" build
```

Connect a 3.3 V USB-UART adapter: adapter RX to **GPIO43**, GND to GND, 115200 baud. USB stays
HID-only either way.

## Flash and recovery

The ESP32-S2 has **no USB-Serial-JTAG**. Once this firmware owns the USB port, `idf.py flash` cannot
auto-reset the chip into the bootloader. Every flash, including the first one, goes like this:

1. Hold the **0** button, tap **RST**, release **0**. The ROM bootloader now enumerates as a serial port.
2. Flash:
   ```bash
   ls /dev/cu.usbmodem*
   idf.py -p /dev/cu.usbmodem* flash
   ```
   (If more than one port is listed, name the one that appeared in step 1, e.g. `/dev/cu.usbmodem01`.)
3. Tap **RST** to start the firmware. The board re-enumerates as "PTT Button".

`idf.py monitor` does not work over USB with this firmware. Use the debug build and a UART adapter.

## Changing the chord

Edit `main/ptt_config.h` and rebuild:

```c
#define PTT_MODIFIERS   (KEYBOARD_MODIFIER_LEFTCTRL)
#define PTT_KEYS        { HID_KEY_GRAVE, 0, 0, 0, 0, 0 }
```

- `PTT_MODIFIERS`: OR together `KEYBOARD_MODIFIER_LEFTCTRL`, `_LEFTSHIFT`, `_LEFTALT` (Option),
  `_LEFTGUI` (Command), `_RIGHTCTRL`, `_RIGHTSHIFT`, `_RIGHTALT`, `_RIGHTGUI`, or `0` for none.
- `PTT_KEYS`: up to 6 non-modifier usage IDs, listed first and padded with `0`.

The build fails with a readable message if there are more than 6 keys, if a modifier usage
(0xE0–0xE7) is in `PTT_KEYS`, if a key comes after a `0`, or if the chord is empty. The checks are in
`main/ptt_config_check.cpp`. They are in C++ because C `static_assert` cannot index an initializer
list.

Useful HID usage IDs (TinyUSB names from `class/hid/hid.h`):

| Key | Usage ID | TinyUSB constant |
|---|---|---|
| ` (grave / tilde) | 0x35 | `HID_KEY_GRAVE` |
| Space | 0x2C | `HID_KEY_SPACE` |
| F13 | 0x68 | `HID_KEY_F13` |
| F14 | 0x69 | `HID_KEY_F14` |
| F15 | 0x6A | `HID_KEY_F15` |
| F16 | 0x6B | `HID_KEY_F16` |
| F17 | 0x6C | `HID_KEY_F17` |
| F18 | 0x6D | `HID_KEY_F18` |
| F19 | 0x6E | `HID_KEY_F19` |
| F20 | 0x6F | `HID_KEY_F20` |
| F21 | 0x70 | `HID_KEY_F21` |
| F22 | 0x71 | `HID_KEY_F22` |
| F23 | 0x72 | `HID_KEY_F23` |
| F24 | 0x73 | `HID_KEY_F24` |
| Right Ctrl, Right Alt, etc. | 0xE4–0xE7 | use `PTT_MODIFIERS` instead |

Examples:

```c
// F18 alone: no modifier, collides with nothing on a Mac keyboard
#define PTT_MODIFIERS   (0)
#define PTT_KEYS        { HID_KEY_F18, 0, 0, 0, 0, 0 }

// Option + Space
#define PTT_MODIFIERS   (KEYBOARD_MODIFIER_LEFTALT)
#define PTT_KEYS        { HID_KEY_SPACE, 0, 0, 0, 0, 0 }
```

With `PTT_MODIFIERS` set to 0, the modifier-only step of the press/release sequence is skipped.

## Binding the dictation app

1. Flash the firmware and plug the board in.
2. In the dictation app's settings, open the push-to-talk or "hold to record" shortcut field, click it,
   then **press and hold the hardware button**. The app records the chord (default `⌃` + `` ` ``).
3. Make sure the app is set to **hold** (record while held, process on release), not toggle.

**F13 to F24 are collision-free alternatives.** Mac keyboards have no F13+ keys and macOS binds nothing
to them, so a single-key chord like F18 cannot clash with a terminal, an IDE or a system shortcut.
`⌃` + `` ` `` toggles the integrated terminal in VS Code and some other editors.

The first time the device is plugged in, macOS may open **Keyboard Setup Assistant**. See the notes on
`bCountryCode` below. If it opens, close it. macOS remembers the device by VID/PID/serial.

Other settings (GPIOs, debounce, delays, max hold, remote wakeup, VID/PID/strings) are in
`idf.py menuconfig` → **PTT Controller**.

## How it works

- `main/ptt_button.c`: an any-edge GPIO interrupt posts `PTT_EVT_BUTTON_EDGE` to a FreeRTOS queue
  (`xQueueSendFromISR` only). The PTT task restarts a debounce window on each edge and accepts the new
  level once the pin has been quiet for `PTT_DEBOUNCE_MS`. The task also resamples the pin every
  50 ms in case an edge was lost to a full queue.
- `main/ptt_hid.c`: esp_tinyusb with a custom device descriptor, configuration descriptor and strings.
  It has one boot-keyboard HID interface (8-byte report, no report ID) on a 1 ms interrupt IN endpoint.
  The serial number is the factory MAC. Mount, unmount, suspend and resume are forwarded from the
  esp_tinyusb event callback to the same queue, and no reports are sent from the USB task. Each report
  waits up to 50 ms for `tud_hid_ready()` and then gives up.
- `main/main.c`: the state machine (`IDLE`/`HELD`), described in SPEC.md §5.
  - Press: modifier-only report, `PTT_MOD_TO_KEY_DELAY_MS`, modifier+keys report, LED on.
  - Release (button, unmount, suspend or max-hold timeout): modifier-only report,
    `PTT_KEY_TO_MOD_DELAY_MS`, all-zero report, LED off.
  - A press is acted on only when it happens while the host is mounted and awake. A button already held
    at boot or at plug-in does nothing until it is released and pressed again.
  - An all-zero report is sent on every mount.

## Acceptance tests (SPEC.md §8)

**Status: flashed and enumerating; tests 2–6 not yet run.** `idf.py build` passes with zero warnings
on IDF 5.5.4 (production and debug variants), and each `ptt_config.h` validation was checked to fail the
build as intended. Test 1 passes on a Lolin S2 mini. Tests 2–6 need someone pressing the button:

| # | Test | How |
|---|---|---|
| 1 | Enumeration ✅ | `ioreg -p IOUSB -l -n "PTT Button" \| grep -E 'idVendor\|idProduct\|USB Vendor Name'`: idVendor 12346 (0x303A), idProduct 16385 (0x4001), vendor "DIY". (On current macOS, `system_profiler SPUSBDataType` did not list the device.) |
| 2 | Ordering | Karabiner-EventViewer: hold the button, expect `left_control` down then `grave_accent_and_tilde` down about 10 ms apart. Release: grave up, then control up |
| 3 | Max hold | Hold for 130 s; the chord releases at 120 s and the LED goes off |
| 4 | Unplug while held | Unplug while holding, replug while still holding: no stuck Ctrl, no chord on replug |
| 5 | Bounce | Tap quickly 20 times: 20 clean down/up pairs |
| 6 | Remote wakeup | Enable `PTT_REMOTE_WAKEUP`, rebuild, sleep the Mac, press the button |

## Open items (flagged, not guessed)

- **esp_tinyusb version/API**: pinned to `==2.4.0`. The registry also has 3.0.0, released
  2026-10-20; it was not used because it is only days old. All calls were checked against the 2.4.0
  headers and IDF 5.5.4's `examples/peripherals/usb/device/tusb_hid`: `tinyusb_driver_install()`,
  `TINYUSB_DEFAULT_CONFIG(event_cb, arg)`, `tinyusb_config_t.descriptor.{device,full_speed_config,string,string_count}`,
  `tinyusb_remote_wakeup()` and `tinyusb_event_t`. esp_tinyusb owns `tud_mount_cb`/`tud_umount_cb`.
  Suspend/resume events come through the event callback because `CONFIG_TINYUSB_SUSPEND_CALLBACK` and
  `CONFIG_TINYUSB_RESUME_CALLBACK` are set in `sdkconfig.defaults`, so the app must not define
  `tud_suspend_cb`/`tud_resume_cb`.
- **HID `bCountryCode`**: esp_tinyusb has no setting for it, and TinyUSB's `TUD_HID_DESCRIPTOR()` macro
  hardcodes 0. `ptt_hid.c` writes the same interface/HID/endpoint bytes by hand with
  `bCountryCode = 33` (US). **Not yet verified** to suppress Keyboard Setup Assistant. If it still
  appears, close it once (see above).
- **Suspend/resume on the S2 OTG peripheral**: TinyUSB's DWC2 driver handles the suspend and resume
  interrupts, and the firmware also polls `tud_suspended()` while held. Whether macOS sleep delivers
  them reliably on the S2 has **not been verified**, so the remote wakeup path (off by default) is
  untested. After `tinyusb_remote_wakeup()` the firmware waits up to 1 s for the bus to resume before
  sending the chord. If it does not resume, the press is dropped.
- **String length**: esp_tinyusb truncates string descriptors to 31 characters. The build rejects
  longer manufacturer/product strings.

## License

MIT, see [LICENSE](LICENSE). The esp_tinyusb and TinyUSB components fetched at build time are
licensed separately (Apache-2.0 and MIT respectively).
