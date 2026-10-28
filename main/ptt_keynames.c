#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "ptt_keynames.h"

#define USAGE_MIN           0x04    // 0x00-0x03 are not keys
#define USAGE_MAX           0xDF    // 0xE0-0xE7 are modifiers, above is reserved
#define USAGE_MOD_FIRST     0xE0
#define USAGE_MOD_LAST      0xE7
#define TOKEN_ECHO_MAX      20      // longest token quoted back in an error message

typedef struct {
    const char *name;
    uint8_t value;
} name_entry_t;

// Parse aliases. The plain forms mean the left-hand key.
static const name_entry_t s_modifiers[] = {
    {"ctrl", PTT_MOD_LCTRL},  {"lctrl", PTT_MOD_LCTRL},   {"rctrl", PTT_MOD_RCTRL},
    {"shift", PTT_MOD_LSHIFT}, {"lshift", PTT_MOD_LSHIFT}, {"rshift", PTT_MOD_RSHIFT},
    {"alt", PTT_MOD_LALT},    {"option", PTT_MOD_LALT},   {"lalt", PTT_MOD_LALT}, {"ralt", PTT_MOD_RALT},
    {"cmd", PTT_MOD_LGUI},    {"gui", PTT_MOD_LGUI},      {"win", PTT_MOD_LGUI},
    {"lcmd", PTT_MOD_LGUI},   {"rcmd", PTT_MOD_RGUI},
};

// Printed names, indexed by modifier bit
static const char *const s_modifier_names[8] = {
    "ctrl", "shift", "alt", "cmd", "rctrl", "rshift", "ralt", "rcmd",
};

// Named keys. A-Z, 0-9 and F1-F24 are handled in code. The first entry for a
// usage is its printed name, later ones are aliases.
static const name_entry_t s_keys[] = {
    {"enter", 0x28}, {"return", 0x28},
    {"esc", 0x29}, {"escape", 0x29},
    {"backspace", 0x2A},
    {"tab", 0x2B},
    {"space", 0x2C},
    {"minus", 0x2D},
    {"equal", 0x2E},
    {"lbracket", 0x2F},
    {"rbracket", 0x30},
    {"backslash", 0x31},
    {"semicolon", 0x33},
    {"quote", 0x34},
    {"grave", 0x35},
    {"comma", 0x36},
    {"period", 0x37},
    {"slash", 0x38},
    {"capslock", 0x39},
    {"printscreen", 0x46},
    {"scrolllock", 0x47},
    {"pause", 0x48},
    {"insert", 0x49},
    {"home", 0x4A},
    {"pageup", 0x4B},
    {"delete", 0x4C},
    {"end", 0x4D},
    {"pagedown", 0x4E},
    {"right", 0x4F},
    {"left", 0x50},
    {"down", 0x51},
    {"up", 0x52},
    {"menu", 0x65},
    {"mute", 0x7F},
    {"volumeup", 0x80},
    {"volumedown", 0x81},
};

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

static bool name_equals(const char *s, size_t len, const char *name)
{
    if (strlen(name) != len) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (tolower((unsigned char)s[i]) != name[i]) {
            return false;
        }
    }
    return true;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static void set_err(char *err, size_t err_size, const char *fmt, ...)
{
    if (err == NULL || err_size == 0) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, err_size, fmt, ap);
    va_end(ap);
}

static int echo_len(size_t len)
{
    return (int)(len > TOKEN_ECHO_MAX ? TOKEN_ECHO_MAX : len);
}

static bool lookup_modifier(const char *s, size_t len, uint8_t *bit)
{
    for (size_t i = 0; i < COUNT_OF(s_modifiers); i++) {
        if (name_equals(s, len, s_modifiers[i].name)) {
            *bit = s_modifiers[i].value;
            return true;
        }
    }
    return false;
}

// 0 if the token is not a key name. Hex tokens are handled by the caller.
static uint8_t lookup_key(const char *s, size_t len)
{
    if (len == 1) {
        const char c = (char)toupper((unsigned char)s[0]);
        if (c >= 'A' && c <= 'Z') {
            return (uint8_t)(0x04 + (c - 'A'));
        }
        if (c >= '1' && c <= '9') {
            return (uint8_t)(0x1E + (c - '1'));
        }
        if (c == '0') {
            return 0x27;
        }
        return 0;
    }
    if ((s[0] == 'F' || s[0] == 'f') && len <= 3 && s[1] >= '1' && s[1] <= '9') {
        int n = s[1] - '0';
        if (len == 3) {
            if (!isdigit((unsigned char)s[2])) {
                return 0;
            }
            n = n * 10 + (s[2] - '0');
        }
        if (n <= 12) {
            return (uint8_t)(0x3A + (n - 1));
        }
        if (n <= 24) {
            return (uint8_t)(0x68 + (n - 13));
        }
        return 0;
    }
    for (size_t i = 0; i < COUNT_OF(s_keys); i++) {
        if (name_equals(s, len, s_keys[i].name)) {
            return s_keys[i].value;
        }
    }
    return 0;
}

// "0x04" to "0xDF": 1 or 2 hex digits. Returns false if the token is not hex at all.
static bool parse_hex(const char *s, size_t len, unsigned *value)
{
    if (len < 3 || len > 4 || s[0] != '0' || (s[1] != 'x' && s[1] != 'X')) {
        return false;
    }
    unsigned v = 0;
    for (size_t i = 2; i < len; i++) {
        const int d = hex_digit(s[i]);
        if (d < 0) {
            return false;
        }
        v = v * 16 + (unsigned)d;
    }
    *value = v;
    return true;
}

static void key_name(uint8_t usage, char *buf, size_t size)
{
    if (usage >= 0x04 && usage <= 0x1D) {
        snprintf(buf, size, "%c", 'A' + (usage - 0x04));
    } else if (usage >= 0x1E && usage <= 0x26) {
        snprintf(buf, size, "%c", '1' + (usage - 0x1E));
    } else if (usage == 0x27) {
        snprintf(buf, size, "0");
    } else if (usage >= 0x3A && usage <= 0x45) {
        snprintf(buf, size, "F%d", 1 + (usage - 0x3A));
    } else if (usage >= 0x68 && usage <= 0x73) {
        snprintf(buf, size, "F%d", 13 + (usage - 0x68));
    } else {
        for (size_t i = 0; i < COUNT_OF(s_keys); i++) {
            if (s_keys[i].value == usage) {
                snprintf(buf, size, "%s", s_keys[i].name);
                return;
            }
        }
        snprintf(buf, size, "0x%02X", usage);
    }
}

bool ptt_chord_parse(const char *text, size_t len, uint8_t *modifiers, uint8_t keys[PTT_CHORD_MAX_KEYS],
                     char *err, size_t err_size)
{
    uint8_t mods = 0;
    uint8_t out[PTT_CHORD_MAX_KEYS] = {0};
    size_t count = 0;
    const char *end = text + len;
    const char *p = text;

    while (p < end && isspace((unsigned char)*p)) {
        p++;
    }
    if (p == end) {
        set_err(err, err_size, "chord is empty");
        return false;
    }

    for (;;) {
        // One token: up to the next '+', with surrounding blanks trimmed
        const char *tok = p;
        while (p < end && *p != '+') {
            p++;
        }
        const char *tok_end = p;
        while (tok < tok_end && isspace((unsigned char)*tok)) {
            tok++;
        }
        while (tok_end > tok && isspace((unsigned char)tok_end[-1])) {
            tok_end--;
        }
        const size_t tok_len = (size_t)(tok_end - tok);

        if (tok_len == 0) {
            set_err(err, err_size, "chord has an empty name (check the + signs)");
            return false;
        }

        uint8_t bit;
        unsigned hex;
        if (lookup_modifier(tok, tok_len, &bit)) {
            mods |= bit;
        } else {
            uint8_t usage;
            if (parse_hex(tok, tok_len, &hex)) {
                if (hex >= USAGE_MOD_FIRST && hex <= USAGE_MOD_LAST) {
                    set_err(err, err_size, "0x%02X is a modifier, use ctrl/shift/alt/cmd instead", hex);
                    return false;
                }
                if (hex < USAGE_MIN || hex > USAGE_MAX) {
                    set_err(err, err_size, "0x%02X is outside 0x04-0xDF", hex);
                    return false;
                }
                usage = (uint8_t)hex;
            } else {
                usage = lookup_key(tok, tok_len);
                if (usage == 0) {
                    set_err(err, err_size, "unknown key \"%.*s\"", echo_len(tok_len), tok);
                    return false;
                }
            }
            for (size_t i = 0; i < count; i++) {
                if (out[i] == usage) {
                    set_err(err, err_size, "key \"%.*s\" is listed twice", echo_len(tok_len), tok);
                    return false;
                }
            }
            if (count == PTT_CHORD_MAX_KEYS) {
                set_err(err, err_size, "more than %d keys", PTT_CHORD_MAX_KEYS);
                return false;
            }
            out[count++] = usage;
        }

        if (p == end) {
            break;
        }
        p++;    // skip '+'
    }

    *modifiers = mods;
    memcpy(keys, out, sizeof(out));
    return true;
}

static void append(char *buf, size_t size, size_t *pos, const char *s)
{
    const size_t n = strlen(s);
    if (*pos < size) {
        const size_t room = size - *pos - 1;
        const size_t copy = n < room ? n : room;
        memcpy(buf + *pos, s, copy);
        buf[*pos + copy] = '\0';
    }
    *pos += n;
}

int ptt_chord_format(uint8_t modifiers, const uint8_t keys[PTT_CHORD_MAX_KEYS], char *buf, size_t size)
{
    size_t pos = 0;
    if (size > 0) {
        buf[0] = '\0';
    }

    for (int bit = 0; bit < 8; bit++) {
        if (modifiers & (1u << bit)) {
            append(buf, size, &pos, pos ? "+" : "");
            append(buf, size, &pos, s_modifier_names[bit]);
        }
    }
    for (int i = 0; i < PTT_CHORD_MAX_KEYS && keys[i] != 0; i++) {
        char name[16];
        key_name(keys[i], name, sizeof(name));
        append(buf, size, &pos, pos ? "+" : "");
        append(buf, size, &pos, name);
    }
    return (int)pos;
}
