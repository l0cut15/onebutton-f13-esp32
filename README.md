# OneButton: USB push-to-talk button (ESP32-S2 mini)

A single physical button for push-to-talk dictation. A Lolin/Wemos S2 mini shows up over USB as a
standard keyboard. While the button is **held**, it holds a key chord (default **Left Ctrl + `**).
When it is released, the chord is released. There is no driver and no host software to install.

It works with any app that has a push-to-talk or "hold to record" shortcut.

- Firmware: ESP-IDF 5.5.4, C, `espressif/esp_tinyusb` 2.4.0
- Case: 3D printed, four parts, no screws ([details](#case))
- Design notes: [SPEC.md](SPEC.md) (firmware), [_Case/OneButton_design_spec.md](_Case/OneButton_design_spec.md) (case)

## Parts

- Lolin/Wemos S2 mini (ESP32-S2)
- Kailh Box Navy switch, or any MX-compatible switch
- Two short lengths of flexible wire
- Printed case parts from [_Case/](_Case/)

## Wiring

```
             Lolin S2 mini
         +-------------------+
         |  [0]       [RST]  |   <- onboard buttons (used for flashing)
         |                   |
         |  GPIO17 o---------+------.
         |                   |      |
         |                   |    --+--   key switch
         |                   |    --+--   (normally open)
         |                   |      |
         |  GND    o---------+------'
         |                   |
         |  GPIO15 = onboard LED (lit while the chord is held)
         |                   |
         +------[USB-C]------+
                   |
                to the computer
```

The internal pull-up is used, so no resistor is needed. GPIO17 sits next to a GND pad, which keeps the
wires short. To use a different pin, change **Button GPIO** in `idf.py menuconfig` → **PTT Controller**.
Pins that cannot work (GPIO0, the USB pins 19/20, the flash pins 26–32) are rejected at build time.

## Build and flash

```bash
. ~/esp/esp-idf-v5.5.4/export.sh
idf.py set-target esp32s2      # once
idf.py build
```

The ESP32-S2 cannot be reset into the bootloader over USB once this firmware is running, so put it in
download mode by hand each time you flash:

1. Hold the **0** button, tap **RST**, release **0**. The board appears as a serial port.
2. Flash:
   ```bash
   idf.py -p /dev/cu.usbmodem* flash
   ```
   If more than one port is listed (`ls /dev/cu.usbmodem*`), use the one that just appeared.
3. Tap **RST**. The board restarts as a keyboard named "PTT Button".

In the case, **0** is reached through the window in the side wall and **RST** through the small hole
on the other side, so the case does not need to be opened.

## Set up your dictation app

1. Plug in the button.
2. In the app's settings, click the push-to-talk or "hold to record" shortcut field, then **press and
   hold the button**. The app records the chord (default `⌃` + `` ` ``).
3. Set the app to **hold** mode (record while held), not toggle.

The first time it is plugged in, macOS may open **Keyboard Setup Assistant**. Close it; macOS
remembers the device after that.

## Changing the key chord

The default `⌃` + `` ` `` also toggles the terminal in VS Code and some other editors. **F13 to F24**
are a good alternative: Mac keyboards do not have them and macOS binds nothing to them.

Edit `main/ptt_config.h`, then rebuild and flash:

```c
#define PTT_MODIFIERS   (KEYBOARD_MODIFIER_LEFTCTRL)
#define PTT_KEYS        { HID_KEY_GRAVE, 0, 0, 0, 0, 0 }
```

- `PTT_MODIFIERS`: OR together `KEYBOARD_MODIFIER_LEFTCTRL`, `_LEFTSHIFT`, `_LEFTALT` (Option),
  `_LEFTGUI` (Command), `_RIGHTCTRL`, `_RIGHTSHIFT`, `_RIGHTALT`, `_RIGHTGUI`, or `0` for none.
- `PTT_KEYS`: up to 6 keys, listed first and padded with `0`. Names are TinyUSB's `HID_KEY_*`
  constants (`class/hid/hid.h`), e.g. `HID_KEY_SPACE`, `HID_KEY_F13` … `HID_KEY_F24`.

The build stops with a clear message if the chord is invalid.

```c
// F18 alone
#define PTT_MODIFIERS   (0)
#define PTT_KEYS        { HID_KEY_F18, 0, 0, 0, 0, 0 }

// Option + Space
#define PTT_MODIFIERS   (KEYBOARD_MODIFIER_LEFTALT)
#define PTT_KEYS        { HID_KEY_SPACE, 0, 0, 0, 0, 0 }
```

Other settings (button and LED pins, debounce, maximum hold time, USB names and IDs) are in
`idf.py menuconfig` → **PTT Controller**.

## Case

About 30 x 39 x 30 mm with the keycap fitted. The STLs are in [_Case/](_Case/).

| Part | File | |
|---|---|---|
| Case shell | `OneButton_CASE_SHELL.stl` | Holds the switch and board; openings for USB-C, BOOT and RST |
| Bottom cap | `OneButton_BOTTOM_CAP.stl` | Snaps into the shell with four latches |
| Keycap | `OneButton_KEYCAP.stl` | 24.3 mm square, MX stem |
| Shroud | `OneButton_SHROUD.stl` | Tapered bezel that guides the keycap so it does not rock; glued on |

**Printing**: PLA, 0.4 mm nozzle, 0.2 mm layers, 3+ perimeters, 15–20% infill, no supports. The STLs
are exported Y-up, so lay each one flat in the slicer: shell top face down, bottom cap floor down,
keycap top down, shroud top face down.

**Assembly**:
1. Solder about 15 mm of wire (plus slack) from each switch pin to the board's GPIO17 and GND pads.
2. Press the switch into the top of the shell.
3. Place the board components-down onto the two pegs inside the shell, USB-C towards the opening.
4. Snap the bottom cap in. To remove it later, press a latch in through its side window.
5. Glue the shroud onto the top using its four pegs, then fit the keycap.

The full case spec, with every dimension and fit, is in
[_Case/OneButton_design_spec.md](_Case/OneButton_design_spec.md). `_Case/onebutton_build.py` is a
Fusion 360 script that rebuilds the model from parameters: run it in an empty design, and set `LOG_PATH`
and `EXPORT_DIR` at the top first.

## Debug logging

The normal build has no console. For logs, build the debug variant and connect a 3.3 V USB-UART
adapter (adapter RX to **GPIO43**, GND to GND, 115200 baud):

```bash
idf.py -B build-debug -D SDKCONFIG=sdkconfig.dbg \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.debug" build
```

## How it works

On press, the firmware sends the modifier alone, waits a few milliseconds, then sends modifier + key, so
apps see the keys in the order a person would press them. On release it does the reverse. The chord is
also released if the USB host disconnects or sleeps, or after a maximum hold time (120 s by default),
so a key can never get stuck down. The button is debounced in software. See [SPEC.md](SPEC.md) for the
full design.

## License

MIT, see [LICENSE](LICENSE). The esp_tinyusb and TinyUSB components fetched at build time are
licensed separately (Apache-2.0 and MIT).
