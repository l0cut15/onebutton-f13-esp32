/*
 * Push-to-talk chord. Edit and rebuild to apply.
 *
 * Timing, GPIOs and USB identity live in Kconfig: `idf.py menuconfig` -> "OneButton".
 */
#pragma once

#include "class/hid/hid.h"

// Modifier bitmask: use TinyUSB KEYBOARD_MODIFIER_* constants, OR them together
#define PTT_MODIFIERS   (0)
// Up to 6 non-modifier HID usage IDs, zero-terminated
#define PTT_KEYS        { HID_KEY_F13, 0, 0, 0, 0, 0 }
