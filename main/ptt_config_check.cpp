/*
 * Compile-time validation of ptt_config.h.
 *
 * C cannot index an initializer list inside static_assert, C++ constexpr can,
 * so this translation unit exists only to fail the build on a bad chord.
 */
#include <cstddef>
#include <cstdint>
#include "ptt_config.h"

namespace {

constexpr uint8_t kKeys[] = PTT_KEYS;
constexpr unsigned kModifiers = PTT_MODIFIERS;

constexpr bool keys_have_no_modifier_usages()
{
    for (uint8_t k : kKeys) {
        if (k >= HID_KEY_CONTROL_LEFT && k <= HID_KEY_GUI_RIGHT) {
            return false;
        }
    }
    return true;
}

constexpr bool keys_are_zero_terminated()
{
    // Once a 0 appears, everything after it must be 0 too.
    bool seen_zero = false;
    for (uint8_t k : kKeys) {
        if (k == 0) {
            seen_zero = true;
        } else if (seen_zero) {
            return false;
        }
    }
    return true;
}

constexpr bool chord_not_empty()
{
    for (uint8_t k : kKeys) {
        if (k != 0) {
            return true;
        }
    }
    return kModifiers != 0;
}

static_assert(sizeof(kKeys) <= 6,
              "ptt_config.h: PTT_KEYS has more than 6 keys (boot keyboard report holds 6)");
static_assert(kModifiers <= 0xFF,
              "ptt_config.h: PTT_MODIFIERS must be a KEYBOARD_MODIFIER_* bitmask (0x00-0xFF)");
static_assert(keys_have_no_modifier_usages(),
              "ptt_config.h: PTT_KEYS contains a modifier usage (0xE0-0xE7); put modifiers in PTT_MODIFIERS");
static_assert(keys_are_zero_terminated(),
              "ptt_config.h: PTT_KEYS has a non-zero key after a 0; list keys first, then pad with 0");
static_assert(chord_not_empty(),
              "ptt_config.h: chord is empty; set at least one modifier or key");

} // namespace
