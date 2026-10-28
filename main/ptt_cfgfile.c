#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "ptt_cfgfile.h"

#define CHORD_TEXT_MAX  96      // longest formatted chord: 8 modifiers + 6 keys

typedef struct {
    const char *p;
    size_t len;
} span_t;

static void add_diag(ptt_cfgfile_result_t *res, unsigned line, bool error, const char *fmt, ...)
{
    if (error) {
        res->error_count++;
    } else {
        res->warning_count++;
    }
    if (res->diag_count == PTT_CFGFILE_MAX_DIAGS) {
        return;
    }
    ptt_diag_t *d = &res->diags[res->diag_count++];
    d->line = line;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(d->msg, sizeof(d->msg), fmt, ap);
    va_end(ap);
}

static span_t trim(const char *p, size_t len)
{
    while (len > 0 && isspace((unsigned char)p[0])) {
        p++;
        len--;
    }
    while (len > 0 && isspace((unsigned char)p[len - 1])) {
        len--;
    }
    return (span_t){p, len};
}

static bool span_equals(span_t s, const char *name)
{
    if (strlen(name) != s.len) {
        return false;
    }
    for (size_t i = 0; i < s.len; i++) {
        if (tolower((unsigned char)s.p[i]) != name[i]) {
            return false;
        }
    }
    return true;
}

static int echo_len(span_t s)
{
    return (int)(s.len > 20 ? 20 : s.len);
}

// Control characters other than tab/CR/LF mean the file is not text. Bytes >= 0x80
// are allowed so UTF-8 comments work.
static bool find_binary(const char *text, size_t len, unsigned *line)
{
    *line = 1;
    for (size_t i = 0; i < len; i++) {
        const unsigned char c = (unsigned char)text[i];
        if (c == '\n') {
            (*line)++;
        } else if ((c < 0x20 && c != '\t' && c != '\r') || c == 0x7F) {
            return true;
        }
    }
    return false;
}

static void parse_max_hold(span_t value, unsigned line, const ptt_settings_t *defaults,
                           ptt_settings_t *out, ptt_cfgfile_result_t *res)
{
    unsigned v = 0;
    bool ok = value.len > 0 && value.len <= 4;
    for (size_t i = 0; ok && i < value.len; i++) {
        ok = isdigit((unsigned char)value.p[i]);
        v = v * 10 + (unsigned)(value.p[i] - '0');
    }
    if (!ok || v > PTT_MAX_HOLD_S_LIMIT) {
        add_diag(res, line, false, "max_hold_s must be 0 to %d, using %u",
                 PTT_MAX_HOLD_S_LIMIT, (unsigned)defaults->max_hold_s);
        return;
    }
    out->max_hold_s = (uint16_t)v;
}

void ptt_cfgfile_parse(const char *text, size_t len, const ptt_settings_t *defaults,
                       ptt_settings_t *out, ptt_cfgfile_result_t *res)
{
    memset(res, 0, sizeof(*res));
    *out = *defaults;

    if (text == NULL) {
        add_diag(res, 0, true, "CONFIG.TXT could not be read");
        res->chord_error = true;
        return;
    }
    if (len > PTT_CFGFILE_MAX_SIZE) {
        add_diag(res, 0, true, "CONFIG.TXT is larger than %d bytes", PTT_CFGFILE_MAX_SIZE);
        res->chord_error = true;
        return;
    }
    unsigned bad_line;
    if (find_binary(text, len, &bad_line)) {
        add_diag(res, bad_line, true, "not a text file");
        res->chord_error = true;
        return;
    }

    // UTF-8 byte order mark, as some Windows editors write
    if (len >= 3 && memcmp(text, "\xEF\xBB\xBF", 3) == 0) {
        text += 3;
        len -= 3;
    }

    unsigned chord_line = 0;
    unsigned max_hold_line = 0;
    unsigned line = 0;
    const char *p = text;
    const char *end = text + len;

    while (p < end) {
        line++;
        const char *eol = memchr(p, '\n', (size_t)(end - p));
        if (eol == NULL) {
            eol = end;
        }
        // '#' starts a comment anywhere on the line
        const char *hash = memchr(p, '#', (size_t)(eol - p));
        const span_t content = trim(p, (size_t)((hash ? hash : eol) - p));
        p = eol < end ? eol + 1 : end;

        if (content.len == 0) {
            continue;
        }
        const char *eq = memchr(content.p, '=', content.len);
        if (eq == NULL) {
            add_diag(res, line, false, "expected name = value, line ignored");
            continue;
        }
        const span_t name = trim(content.p, (size_t)(eq - content.p));
        const span_t value = trim(eq + 1, (size_t)(content.p + content.len - (eq + 1)));

        if (span_equals(name, "chord")) {
            if (chord_line != 0) {
                add_diag(res, line, false, "chord is already set on line %u, ignored", chord_line);
                continue;
            }
            chord_line = line;
            char err[64];
            if (!ptt_chord_parse(value.p, value.len, &out->modifiers, out->keys, err, sizeof(err))) {
                add_diag(res, line, true, "%s", err);
                res->chord_error = true;
            }
        } else if (span_equals(name, "max_hold_s")) {
            if (max_hold_line != 0) {
                add_diag(res, line, false, "max_hold_s is already set on line %u, ignored", max_hold_line);
                continue;
            }
            max_hold_line = line;
            parse_max_hold(value, line, defaults, out, res);
        } else if (name.len == 0) {
            add_diag(res, line, false, "expected name = value, line ignored");
        } else {
            add_diag(res, line, false, "unknown setting \"%.*s\", ignored", echo_len(name), name.p);
        }
    }

    if (chord_line == 0) {
        add_diag(res, 0, true, "chord is missing");
        res->chord_error = true;
    }
    if (res->chord_error) {
        // A failed parse may have written part of a chord
        out->modifiers = defaults->modifiers;
        memcpy(out->keys, defaults->keys, sizeof(out->keys));
    }
}

static void bprintf(char *buf, size_t size, size_t *pos, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(*pos < size ? buf + *pos : NULL, *pos < size ? size - *pos : 0, fmt, ap);
    va_end(ap);
    if (n > 0) {
        *pos += (size_t)n;
    }
}

size_t ptt_cfgfile_format_default(const ptt_settings_t *defaults, char *buf, size_t size)
{
    char chord[CHORD_TEXT_MAX];
    ptt_chord_format(defaults->modifiers, defaults->keys, chord, sizeof(chord));

    size_t pos = 0;
    if (size > 0) {
        buf[0] = '\0';
    }
    bprintf(buf, size, &pos,
            "# OneButton F13 configuration\n"
            "# Edit, save, then eject the ONEBUTTON drive to apply.\n"
            "# STATUS.TXT shows what the device made of this file.\n"
            "#\n"
            "# chord: modifiers and keys joined by +, e.g. ctrl+shift+F13, alt+space, F15\n"
            "#   modifiers: ctrl, shift, alt (or option), cmd (or gui, win)\n"
            "#              rctrl, rshift, ralt, rcmd for the right-hand keys\n"
            "#   keys:      A-Z, 0-9, F1-F24, space, enter, tab, esc, ...\n"
            "#              or a HID usage ID in hex, e.g. 0x68. Full list in the README.\n"
            "\n"
            "chord = %s\n"
            "\n"
            "# Release the chord after this many seconds held (0 = never)\n"
            "# max_hold_s = %u\n",
            chord, (unsigned)defaults->max_hold_s);
    return pos;
}

size_t ptt_cfgfile_format_status(const ptt_settings_t *settings, const ptt_cfgfile_result_t *res,
                                 const ptt_status_info_t *info, char *buf, size_t size)
{
    size_t pos = 0;
    if (size > 0) {
        buf[0] = '\0';
    }

    bprintf(buf, size, &pos, "%s status\n", info->product);
    bprintf(buf, size, &pos, "firmware: %s\n", info->version);
    if (info->reformatted) {
        bprintf(buf, size, &pos, "storage: was blank or damaged, reformatted\n");
    }
    if (info->wrote_default) {
        bprintf(buf, size, &pos, "CONFIG.TXT: was missing, default written\n");
    }

    if (res->chord_error) {
        bprintf(buf, size, &pos, "config: ERROR, using factory default\n");
    } else if (res->warning_count > 0) {
        bprintf(buf, size, &pos, "config: OK, %u warning%s\n",
                (unsigned)res->warning_count, res->warning_count == 1 ? "" : "s");
    } else {
        bprintf(buf, size, &pos, "config: OK\n");
    }
    for (size_t i = 0; i < res->diag_count; i++) {
        if (res->diags[i].line > 0) {
            bprintf(buf, size, &pos, "line %u: %s\n", res->diags[i].line, res->diags[i].msg);
        } else {
            bprintf(buf, size, &pos, "%s\n", res->diags[i].msg);
        }
    }
    const size_t total = res->error_count + res->warning_count;
    if (total > res->diag_count) {
        bprintf(buf, size, &pos, "(%u more not shown)\n", (unsigned)(total - res->diag_count));
    }

    char chord[CHORD_TEXT_MAX];
    ptt_chord_format(settings->modifiers, settings->keys, chord, sizeof(chord));
    bprintf(buf, size, &pos, "chord: %s\n", chord);
    if (settings->max_hold_s == 0) {
        bprintf(buf, size, &pos, "max_hold_s: 0 (unlimited)\n");
    } else {
        bprintf(buf, size, &pos, "max_hold_s: %u\n", (unsigned)settings->max_hold_s);
    }
    return pos;
}
