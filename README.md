# OneButton F13: one-button F13 key for ESP32-S2

A single physical key that sends **F13**. A Lolin/Wemos S2 mini shows up over USB as a standard
keyboard. While the button is **held**, F13 is held down; when it is released, F13 is released. There
is no driver and no host software to install.

Mac keyboards have no F13 key and macOS binds nothing to it, so it never clashes with an existing
shortcut. Bind it to anything that accepts a keyboard shortcut. The key can be changed to any other
key or key combination by editing a text file on the device: hold **BOOT** and it shows up as a
small USB drive ([details](#changing-the-key)).

- Firmware: ESP-IDF 5.5.4, C, `espressif/esp_tinyusb` 2.4.0
- Prebuilt firmware 2.0.0: [firmware/onebutton-f13-esp32-s2.bin](firmware/onebutton-f13-esp32-s2.bin) ([flash it without ESP-IDF](#flash-the-prebuilt-firmware))
- Case: 3D printed, four parts, no screws ([details](#case))
- Design notes: [SPEC.md](SPEC.md) and [SPEC_v2.md](SPEC_v2.md) (firmware), [_Case/OneButton_design_spec.md](_Case/OneButton_design_spec.md) (case)

![OneButton F13 in its 3D printed case](_photo/case.jpeg)

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
         |  GPIO15 = onboard LED (lit while the key is held)
         |                   |
         +------[USB-C]------+
                   |
                to the computer
```

The internal pull-up is used, so no resistor is needed. GPIO17 sits next to a GND pad, which keeps the
wires short. To use a different pin, change **Button GPIO** in `idf.py menuconfig` → **OneButton**.
Pins that cannot work (GPIO0, the USB pins 19/20, the flash pins 26–32) are rejected at build time.

## Flash the prebuilt firmware

[firmware/onebutton-f13-esp32-s2.bin](firmware/onebutton-f13-esp32-s2.bin) is a ready-made image
of firmware 2.0.0 with the default settings: button on **GPIO17**, LED on GPIO15, config drive on
**BOOT**, key **F13**. It is a single merged file (bootloader, partition table and app) written at
offset **0x0**. No ESP-IDF install is needed.

1. Put the board in download mode: hold **0**, tap **RST**, release **0**.
2. Open the [Espressif web flasher](https://espressif.github.io/esptool-js/) in Chrome or Edge
   (it needs Web Serial, which Safari and Firefox do not support).
3. Click **Connect** and pick the board's serial port.
4. Set **Flash Address** to `0x0`, choose `onebutton-f13-esp32-s2.bin`, and click **Program**.
   Do not click **Erase Flash**: it also erases your settings.
5. When it finishes, tap **RST**. The board restarts as a keyboard named "OneButton F13".

Or from a terminal, with Python's esptool (`pip install esptool`):

```bash
esptool.py --chip esp32s2 write_flash 0x0 firmware/onebutton-f13-esp32-s2.bin
```

The key can be changed afterwards without rebuilding ([Changing the key](#changing-the-key)). To
change the button pin, build from source.

Reflashing the image keeps your settings: they live in a separate `storage` partition that the image
does not cover. `esptool.py erase_flash` (or **Erase Flash** in the web flasher) wipes them, and the
next boot writes a default `CONFIG.TXT`.

Upgrading from 1.x needs nothing special: flash the image the same way. The first start formats the
`storage` partition and writes a default `CONFIG.TXT` with F13. A chord set in `ptt_config.h` for a
1.x build is not carried over; set it again in `CONFIG.TXT`.

![Lolin S2 mini wired to the switch](_photo/board.jpeg)

## Build and flash from source

```bash
. ~/esp/esp-idf-v5.5.4/export.sh
idf.py set-target esp32s2      # once
idf.py build
```

The ESP32-S2 cannot be reset into the bootloader over USB once this firmware is running, so put it in
download mode by hand each time you flash:

1. Hold the **0** button, tap **RST**, release **0**. The board appears as a serial port.
   (Holding **0** while the board powers up or resets always enters this download mode; holding it
   while the firmware is running opens the [config drive](#changing-the-key) instead.)
2. Flash:
   ```bash
   idf.py -p /dev/cu.usbmodem* flash
   ```
   If more than one port is listed (`ls /dev/cu.usbmodem*`), use the one that just appeared.
3. Tap **RST**. The board restarts as a keyboard named "OneButton F13".

In the case, **0** is reached through the window in the side wall and **RST** through the small hole
on the other side, so the case does not need to be opened.

## Bind it in an app

1. Plug in the button.
2. In the app's settings, click the shortcut field, then **press the button**. The app records F13.
3. If the app offers hold or toggle, pick **hold** to keep it active only while the button is down.

The first time it is plugged in, macOS may open **Keyboard Setup Assistant**. Close it; macOS
remembers the device after that.

## Changing the key

1. Hold the board's **BOOT** button (**0**, reachable through the side window of the case) for one
   second. The device restarts as a USB drive named **ONEBUTTON**, and the LED blinks slowly.
2. Open `CONFIG.TXT` on the drive, change the `chord` line and save:
   ```
   chord = ctrl+shift+F15
   ```
3. **Eject** the drive (Finder, or drag it to the bin). The device restarts as a keyboard with the
   new chord, about a second later.

Holding **BOOT** for a second also switches back to the keyboard. Eject first even then: otherwise
the computer may not have finished writing the file. Unplugging also works, with the same caveat.
The next power-up is always a keyboard.

`STATUS.TXT` on the drive shows what the device made of the file:

```
OneButton F13 status
firmware: 2.0.0
config: ERROR, using factory default
line 4: unknown key "F31"
chord: F13
max_hold_s: 120
```

If `CONFIG.TXT` has an error, the device uses the factory chord (F13) and blinks its LED three
times when it starts. Deleting `CONFIG.TXT` resets it: the next start writes a fresh default file.

### CONFIG.TXT

One `name = value` per line. `#` starts a comment. Names and key names are not case-sensitive.

| Setting | Value | Default |
|---|---|---|
| `chord` | Modifiers and keys joined by `+`, at most 6 keys | `F13` |
| `max_hold_s` | Release the chord after this many seconds held, 0 to 3600, 0 = never | `120` |

**Modifiers**: `ctrl`, `shift`, `alt` (or `option`), `cmd` (or `gui`, `win`). These mean the
left-hand key; `lctrl`, `lshift`, `lalt`, `lcmd` say so explicitly, and `rctrl`, `rshift`, `ralt`,
`rcmd` are the right-hand ones. A chord can be modifiers only, e.g. `ctrl`.

**Keys**:

| Names | |
|---|---|
| `A` – `Z`, `0` – `9`, `F1` – `F24` | |
| `space`, `enter` (`return`), `tab`, `esc` (`escape`), `backspace`, `delete`, `insert` | |
| `home`, `end`, `pageup`, `pagedown`, `up`, `down`, `left`, `right` | |
| `grave` (`` ` ``), `minus`, `equal`, `lbracket`, `rbracket`, `backslash`, `semicolon`, `quote`, `comma`, `period`, `slash` | US layout names |
| `capslock`, `printscreen`, `scrolllock`, `pause`, `menu`, `mute`, `volumeup`, `volumedown` | |
| `0x04` – `0xDF` | Any other key, as a HID usage ID (keyboard page) |

Examples: `ctrl+grave`, `alt+space`, `cmd+shift+F15`, `0x64`.

### Factory default

The factory chord, used when `CONFIG.TXT` is missing or invalid, is set in `main/ptt_config.h` and
needs a rebuild:

```c
#define PTT_MODIFIERS   (0)
#define PTT_KEYS        { HID_KEY_F13, 0, 0, 0, 0, 0 }
```

- `PTT_MODIFIERS`: OR together `KEYBOARD_MODIFIER_LEFTCTRL`, `_LEFTSHIFT`, `_LEFTALT` (Option),
  `_LEFTGUI` (Command), `_RIGHTCTRL`, `_RIGHTSHIFT`, `_RIGHTALT`, `_RIGHTGUI`, or `0` for none.
- `PTT_KEYS`: up to 6 keys, listed first and padded with `0`. Names are TinyUSB's `HID_KEY_*`
  constants (`class/hid/hid.h`), e.g. `HID_KEY_SPACE`, `HID_KEY_F13` … `HID_KEY_F24`.

The build stops with a clear message if the combination is invalid.

Other settings (button and LED pins, the config button and its hold time, debounce, USB names and
IDs) are in `idf.py menuconfig` → **OneButton**. Setting **Config button GPIO** to -1 turns the
config drive off.

The parser has host-side unit tests: `make -C test`.

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
   The pegs pass through the board's mount holes and stand about 3 mm proud: heat-stake them with a
   hot tool (soldering iron) to hold the board.
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

The firmware presses the key when the button goes down and releases it when the button comes up. If
a modifier is configured, it is pressed a few milliseconds before the key and released a few
milliseconds after it, the way a person would type it. The key is also released if the USB host
disconnects or sleeps, or after a maximum hold time (120 s by default), so it can never get stuck down.
The button is debounced in software. See [SPEC.md](SPEC.md) for the full design.

The chord is read from `CONFIG.TXT` on a 1 MB FAT partition at every start, before USB comes up.
Holding BOOT sets a flag in RTC memory and restarts; the next start sees the flag and enumerates as a
USB drive (its own product ID, 0x4002) instead of a keyboard. Ejecting the drive restarts it as a
keyboard. Keyboard and drive are never active at the same time. See [SPEC_v2.md](SPEC_v2.md).

## License

MIT, see [LICENSE](LICENSE). The esp_tinyusb and TinyUSB components fetched at build time are
licensed separately (Apache-2.0 and MIT).
