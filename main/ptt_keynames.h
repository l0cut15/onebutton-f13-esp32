/*
 * Key and modifier names <-> HID usage IDs (keyboard page 0x07), and chord text.
 *
 * The only place key names are defined. Plain C with no ESP-IDF or TinyUSB
 * dependency, so it builds on the host for unit tests (test/).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PTT_CHORD_MAX_KEYS  6       // boot keyboard report holds 6 keys

// HID modifier byte bits (same values as TinyUSB's KEYBOARD_MODIFIER_*)
#define PTT_MOD_LCTRL   0x01
#define PTT_MOD_LSHIFT  0x02
#define PTT_MOD_LALT    0x04
#define PTT_MOD_LGUI    0x08
#define PTT_MOD_RCTRL   0x10
#define PTT_MOD_RSHIFT  0x20
#define PTT_MOD_RALT    0x40
#define PTT_MOD_RGUI    0x80

/**
 * Parse chord text such as "ctrl+shift+F13".
 *
 * @param[out] modifiers HID modifier byte
 * @param[out] keys      Usage IDs, listed first and padded with 0
 * @param[out] err       Readable reason on failure (may be NULL)
 * @return true if the chord is valid
 */
bool ptt_chord_parse(const char *text, size_t len, uint8_t *modifiers, uint8_t keys[PTT_CHORD_MAX_KEYS],
                     char *err, size_t err_size);

/**
 * Format a chord the way ptt_chord_parse() reads it: modifiers first, then keys.
 * Keys without a name are printed as hex ("0x64").
 *
 * @return Length of the full text (snprintf semantics)
 */
int ptt_chord_format(uint8_t modifiers, const uint8_t keys[PTT_CHORD_MAX_KEYS], char *buf, size_t size);
