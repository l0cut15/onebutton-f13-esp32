/*
 * Push-to-talk chord. Edit and rebuild to apply.
 *
 * Timing, GPIOs and USB identity live in Kconfig: `idf.py menuconfig` -> "PTT Controller".
 */
#pragma once

#include "class/hid/hid.h"

// Modifier bitmask: use TinyUSB KEYBOARD_MODIFIER_* constants, OR them together
#define PTT_MODIFIERS   (KEYBOARD_MODIFIER_LEFTCTRL)
// Up to 6 non-modifier HID usage IDs, zero-terminated
#define PTT_KEYS        { HID_KEY_GRAVE, 0, 0, 0, 0, 0 }
