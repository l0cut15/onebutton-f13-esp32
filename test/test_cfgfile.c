/*
 * Host unit tests for ptt_cfgfile.c and ptt_keynames.c. Run: make -C test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ptt_cfgfile.h"
#include "ptt_keynames.h"

static int s_failures;
static int s_checks;

#define CHECK(cond, ...)                                            \
    do {                                                            \
        s_checks++;                                                 \
        if (!(cond)) {                                              \
            s_failures++;                                           \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);             \
            printf(__VA_ARGS__);                                    \
            printf("\n");                                           \
        }                                                           \
    } while (0)

// Factory default as in ptt_config.h: F13, no modifier, 120 s
static const ptt_settings_t kFactory = {
    .modifiers = 0,
    .keys = {0x68, 0, 0, 0, 0, 0},
    .max_hold_s = 120,
};

/* ---------------- Chord grammar ---------------- */

typedef struct {
    const char *text;
    uint8_t modifiers;
    uint8_t keys[PTT_CHORD_MAX_KEYS];
    const char *formatted;      // ptt_chord_format() of the result
} good_chord_t;

static const good_chord_t kGoodChords[] = {
    {"F13", 0, {0x68}, "F13"},
    {"f13", 0, {0x68}, "F13"},
    {"F1", 0, {0x3A}, "F1"},
    {"F12", 0, {0x45}, "F12"},
    {"F24", 0, {0x73}, "F24"},
    {"ctrl+F13", PTT_MOD_LCTRL, {0x68}, "ctrl+F13"},
    {"ctrl+shift+F15", PTT_MOD_LCTRL | PTT_MOD_LSHIFT, {0x6A}, "ctrl+shift+F15"},
    {" shift + ctrl + F15 ", PTT_MOD_LCTRL | PTT_MOD_LSHIFT, {0x6A}, "ctrl+shift+F15"},
    {"alt+space", PTT_MOD_LALT, {0x2C}, "alt+space"},
    {"option+SPACE", PTT_MOD_LALT, {0x2C}, "alt+space"},
    {"cmd+a", PTT_MOD_LGUI, {0x04}, "cmd+A"},
    {"gui+z", PTT_MOD_LGUI, {0x1D}, "cmd+Z"},
    {"win+1", PTT_MOD_LGUI, {0x1E}, "cmd+1"},
    {"lcmd+0", PTT_MOD_LGUI, {0x27}, "cmd+0"},
    {"rctrl+rshift+ralt+rcmd+9", 0xF0, {0x26}, "rctrl+rshift+ralt+rcmd+9"},
    {"lctrl+lshift+lalt", 0x07, {0}, "ctrl+shift+alt"},
    {"ctrl", PTT_MOD_LCTRL, {0}, "ctrl"},
    {"ctrl+ctrl+grave", PTT_MOD_LCTRL, {0x35}, "ctrl+grave"},
    {"return", 0, {0x28}, "enter"},
    {"escape", 0, {0x29}, "esc"},
    {"a+b+c+d+e+f", 0, {0x04, 0x05, 0x06, 0x07, 0x08, 0x09}, "A+B+C+D+E+F"},
    {"0x04", 0, {0x04}, "A"},
    {"0xdf", 0, {0xDF}, "0xDF"},
    {"0X64", 0, {0x64}, "0x64"},
    {"0x4", 0, {0x04}, "A"},
    {"tab+backspace+delete+insert+home+end", 0, {0x2B, 0x2A, 0x4C, 0x49, 0x4A, 0x4D},
     "tab+backspace+delete+insert+home+end"},
    {"pageup+pagedown+up+down+left+right", 0, {0x4B, 0x4E, 0x52, 0x51, 0x50, 0x4F},
     "pageup+pagedown+up+down+left+right"},
    {"minus+equal+lbracket+rbracket+backslash+semicolon", 0, {0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x33},
     "minus+equal+lbracket+rbracket+backslash+semicolon"},
    {"quote+comma+period+slash+capslock+printscreen", 0, {0x34, 0x36, 0x37, 0x38, 0x39, 0x46},
     "quote+comma+period+slash+capslock+printscreen"},
    {"scrolllock+pause+menu+mute+volumeup+volumedown", 0, {0x47, 0x48, 0x65, 0x7F, 0x80, 0x81},
     "scrolllock+pause+menu+mute+volumeup+volumedown"},
};

typedef struct {
    const char *text;
    const char *error;          // expected substring of the error message
} bad_chord_t;

static const bad_chord_t kBadChords[] = {
    {"", "chord is empty"},
    {"   ", "chord is empty"},
    {"F31", "unknown key \"F31\""},
    {"F0", "unknown key"},
    {"F25", "unknown key"},
    {"F013", "unknown key"},
    {"ctrl+", "empty name"},
    {"+F13", "empty name"},
    {"ctrl++F13", "empty name"},
    {"a+b+c+d+e+f+g", "more than 6 keys"},
    {"0xE0", "0xE0 is a modifier"},
    {"0xe7", "0xE7 is a modifier"},
    {"0xE8", "outside 0x04-0xDF"},
    {"0x03", "outside 0x04-0xDF"},
    {"0x00", "outside 0x04-0xDF"},
    {"0x", "unknown key"},
    {"0x123", "unknown key"},
    {"0xZZ", "unknown key"},
    {"a+A", "listed twice"},
    {"ctrl F13", "unknown key \"ctrl F13\""},
    {"lgui", "unknown key"},
    {"control", "unknown key"},
    {"averyveryverylongkeynamethatgoesonandon", "unknown key \"averyveryverylongkey\""},
};

static void test_chords(void)
{
    for (size_t i = 0; i < sizeof(kGoodChords) / sizeof(kGoodChords[0]); i++) {
        const good_chord_t *t = &kGoodChords[i];
        uint8_t mods = 0xAA;
        uint8_t keys[PTT_CHORD_MAX_KEYS];
        char err[64] = "";
        const bool ok = ptt_chord_parse(t->text, strlen(t->text), &mods, keys, err, sizeof(err));
        CHECK(ok, "\"%s\" rejected: %s", t->text, err);
        if (!ok) {
            continue;
        }
        CHECK(mods == t->modifiers, "\"%s\" modifiers 0x%02X, want 0x%02X", t->text, mods, t->modifiers);
        CHECK(memcmp(keys, t->keys, sizeof(keys)) == 0, "\"%s\" keys differ", t->text);

        char buf[96];
        ptt_chord_format(mods, keys, buf, sizeof(buf));
        CHECK(strcmp(buf, t->formatted) == 0, "\"%s\" formats as \"%s\", want \"%s\"", t->text, buf, t->formatted);

        // Formatting must round-trip through the parser
        uint8_t mods2;
        uint8_t keys2[PTT_CHORD_MAX_KEYS];
        CHECK(ptt_chord_parse(buf, strlen(buf), &mods2, keys2, NULL, 0) && mods2 == mods &&
              memcmp(keys2, keys, sizeof(keys)) == 0, "\"%s\" does not round-trip", buf);
    }

    for (size_t i = 0; i < sizeof(kBadChords) / sizeof(kBadChords[0]); i++) {
        const bad_chord_t *t = &kBadChords[i];
        uint8_t mods;
        uint8_t keys[PTT_CHORD_MAX_KEYS];
        char err[64] = "";
        const bool ok = ptt_chord_parse(t->text, strlen(t->text), &mods, keys, err, sizeof(err));
        CHECK(!ok, "\"%s\" accepted", t->text);
        CHECK(strstr(err, t->error) != NULL, "\"%s\" error \"%s\", want \"%s\"", t->text, err, t->error);
    }
}

static void test_every_usage_round_trips(void)
{
    for (unsigned u = 0x04; u <= 0xDF; u++) {
        const uint8_t keys[PTT_CHORD_MAX_KEYS] = {(uint8_t)u};
        char buf[32];
        ptt_chord_format(0, keys, buf, sizeof(buf));
        uint8_t mods;
        uint8_t keys2[PTT_CHORD_MAX_KEYS];
        const bool ok = ptt_chord_parse(buf, strlen(buf), &mods, keys2, NULL, 0);
        CHECK(ok && mods == 0 && keys2[0] == u && keys2[1] == 0, "usage 0x%02X (\"%s\") does not round-trip", u, buf);
    }
    for (unsigned bit = 0; bit < 8; bit++) {
        const uint8_t keys[PTT_CHORD_MAX_KEYS] = {0};
        char buf[32];
        ptt_chord_format((uint8_t)(1u << bit), keys, buf, sizeof(buf));
        uint8_t mods;
        uint8_t keys2[PTT_CHORD_MAX_KEYS];
        CHECK(ptt_chord_parse(buf, strlen(buf), &mods, keys2, NULL, 0) && mods == (1u << bit),
              "modifier bit %u (\"%s\") does not round-trip", bit, buf);
    }
}

static void test_format_truncation(void)
{
    const uint8_t keys[PTT_CHORD_MAX_KEYS] = {0x68};
    char buf[5];
    const int n = ptt_chord_format(PTT_MOD_LCTRL, keys, buf, sizeof(buf));
    CHECK(n == (int)strlen("ctrl+F13"), "format length %d", n);
    CHECK(strcmp(buf, "ctrl") == 0, "truncated to \"%s\"", buf);
}

/* ---------------- CONFIG.TXT ---------------- */

typedef struct {
    const char *name;
    const char *text;           // NULL = unreadable
    bool chord_error;
    uint8_t modifiers;
    uint8_t key0;
    uint16_t max_hold_s;
    size_t warnings;
    const char *status_line;    // expected substring of STATUS.TXT, or NULL
} file_case_t;

static const file_case_t kFiles[] = {
    {"minimal", "chord = ctrl+F13\n", false, PTT_MOD_LCTRL, 0x68, 120, 0, "config: OK\n"},
    {"crlf", "# c\r\nchord = ctrl+shift+F15\r\nmax_hold_s = 30\r\n", false, 0x03, 0x6A, 30, 0, "max_hold_s: 30\n"},
    {"no trailing newline", "chord=F14", false, 0, 0x69, 120, 0, "chord: F14\n"},
    {"bom", "\xEF\xBB\xBF" "chord = F14\n", false, 0, 0x69, 120, 0, "config: OK\n"},
    {"utf8 comment", "# Größe ✓\nchord = F14 # trailing comment\n", false, 0, 0x69, 120, 0, "config: OK\n"},
    {"name case and spaces", "\t CHORD\t=\tF14  \n  Max_Hold_S = 0\n", false, 0, 0x69, 0, 0, "max_hold_s: 0 (unlimited)\n"},
    {"max hold 3600", "chord = F14\nmax_hold_s = 3600\n", false, 0, 0x69, 3600, 0, NULL},
    {"unknown setting", "chord = F14\ncolour = blue\n", false, 0, 0x69, 120, 1,
     "line 2: unknown setting \"colour\", ignored\n"},
    {"no equals", "chord = F14\nhello\n", false, 0, 0x69, 120, 1, "line 2: expected name = value"},
    {"empty name", "chord = F14\n = 3\n", false, 0, 0x69, 120, 1, "line 2: expected name = value"},
    {"bad max hold", "chord = F14\nmax_hold_s = 3601\n", false, 0, 0x69, 120, 1,
     "line 2: max_hold_s must be 0 to 3600, using 120\n"},
    {"bad max hold text", "chord = F14\nmax_hold_s = -1\n", false, 0, 0x69, 120, 1, "config: OK, 1 warning\n"},
    {"bad max hold empty", "chord = F14\nmax_hold_s =\n", false, 0, 0x69, 120, 1, NULL},
    {"duplicate chord", "chord = F14\nchord = F15\n", false, 0, 0x69, 120, 1,
     "line 2: chord is already set on line 1, ignored\n"},
    {"unknown key", "# a\n# b\n\nchord = F31\n", true, 0, 0x68, 120, 0,
     "config: ERROR, using factory default\nline 4: unknown key \"F31\"\nchord: F13\n"},
    {"seven keys", "chord = a+b+c+d+e+f+g\n", true, 0, 0x68, 120, 0, "line 1: more than 6 keys\n"},
    {"0xE0", "chord = 0xE0\n", true, 0, 0x68, 120, 0, "line 1: 0xE0 is a modifier"},
    {"chord error keeps max hold", "chord = nope\nmax_hold_s = 10\n", true, 0, 0x68, 10, 0, "max_hold_s: 10\n"},
    {"partial chord discarded", "chord = ctrl+a+nope\n", true, 0, 0x68, 120, 0, "chord: F13\n"},
    {"empty file", "", true, 0, 0x68, 120, 0, "config: ERROR, using factory default\nchord is missing\n"},
    {"only comments", "# chord = F14\n", true, 0, 0x68, 120, 0, "chord is missing\n"},
    {"empty chord", "chord =\n", true, 0, 0x68, 120, 0, "line 1: chord is empty\n"},
    {"unreadable", NULL, true, 0, 0x68, 120, 0, "CONFIG.TXT could not be read\n"},
    {"binary junk", "chord = F14\n\x01\x02\xff\x00junk", true, 0, 0x68, 120, 0, "line 2: not a text file\n"},
};

static void run_file_case(const file_case_t *t, size_t len)
{
    ptt_settings_t s;
    ptt_cfgfile_result_t res;
    ptt_cfgfile_parse(t->text, len, &kFactory, &s, &res);

    CHECK(res.chord_error == t->chord_error, "[%s] chord_error %d", t->name, res.chord_error);
    CHECK(s.modifiers == t->modifiers, "[%s] modifiers 0x%02X", t->name, s.modifiers);
    CHECK(s.keys[0] == t->key0, "[%s] key 0x%02X", t->name, s.keys[0]);
    CHECK(s.max_hold_s == t->max_hold_s, "[%s] max_hold_s %u", t->name, s.max_hold_s);
    CHECK(res.warning_count == t->warnings, "[%s] %zu warnings", t->name, res.warning_count);
    CHECK(res.chord_error == (res.error_count > 0), "[%s] error count %zu", t->name, res.error_count);

    char status[1024];
    const ptt_status_info_t info = {.product = "OneButton F13", .version = "2.0.0"};
    const size_t n = ptt_cfgfile_format_status(&s, &res, &info, status, sizeof(status));
    CHECK(n < sizeof(status), "[%s] status truncated", t->name);
    CHECK(strncmp(status, "OneButton F13 status\nfirmware: 2.0.0\n", 37) == 0, "[%s] status header:\n%s", t->name, status);
    if (t->status_line) {
        CHECK(strstr(status, t->status_line) != NULL, "[%s] STATUS.TXT lacks \"%s\":\n%s", t->name, t->status_line, status);
    }
}

static void test_files(void)
{
    for (size_t i = 0; i < sizeof(kFiles) / sizeof(kFiles[0]); i++) {
        const file_case_t *t = &kFiles[i];
        // binary junk contains a NUL, so its length is given explicitly
        size_t len = t->text ? strlen(t->text) : 0;
        if (strcmp(t->name, "binary junk") == 0) {
            len = sizeof("chord = F14\n\x01\x02\xff\x00junk") - 1;
        }
        run_file_case(t, len);
    }
}

static void test_oversize_file(void)
{
    // 10 KB of otherwise valid text
    const size_t len = 10 * 1024;
    char *text = malloc(len);
    memset(text, '#', len);
    memcpy(text, "chord = F14\n", 12);
    text[len - 1] = '\n';

    ptt_settings_t s;
    ptt_cfgfile_result_t res;
    ptt_cfgfile_parse(text, len, &kFactory, &s, &res);
    CHECK(res.chord_error && s.keys[0] == 0x68, "10 KB file accepted");
    CHECK(res.diag_count == 1 && strstr(res.diags[0].msg, "larger than 4096") != NULL, "10 KB file diag");

    // Exactly the limit is fine
    ptt_cfgfile_parse(text, PTT_CFGFILE_MAX_SIZE, &kFactory, &s, &res);
    CHECK(!res.chord_error && s.keys[0] == 0x69, "4096-byte file rejected");
    free(text);
}

static void test_many_diags(void)
{
    char text[512] = "chord = F14\n";
    for (int i = 0; i < 12; i++) {
        strcat(text, "x = 1\n");
    }
    ptt_settings_t s;
    ptt_cfgfile_result_t res;
    ptt_cfgfile_parse(text, strlen(text), &kFactory, &s, &res);
    CHECK(res.warning_count == 12 && res.diag_count == PTT_CFGFILE_MAX_DIAGS, "diag overflow %zu/%zu",
          res.warning_count, res.diag_count);

    char status[1024];
    const ptt_status_info_t info = {.product = "OneButton F13", .version = "2.0.0"};
    ptt_cfgfile_format_status(&s, &res, &info, status, sizeof(status));
    CHECK(strstr(status, "config: OK, 12 warnings\n") && strstr(status, "(4 more not shown)\n"), "overflow status:\n%s", status);
}

static void test_status_notes(void)
{
    ptt_settings_t s;
    ptt_cfgfile_result_t res;
    ptt_cfgfile_parse("chord = F13\n", 12, &kFactory, &s, &res);
    const ptt_status_info_t info = {.product = "X", .version = "1", .reformatted = true, .wrote_default = true};
    char status[512];
    ptt_cfgfile_format_status(&s, &res, &info, status, sizeof(status));
    CHECK(strcmp(status,
                 "X status\n"
                 "firmware: 1\n"
                 "storage: was blank or damaged, reformatted\n"
                 "CONFIG.TXT: was missing, default written\n"
                 "config: OK\n"
                 "chord: F13\n"
                 "max_hold_s: 120\n") == 0, "status notes:\n%s", status);
}

// Acceptance test 3: the default file parses with no warnings, and gives back the defaults
static void test_default_file(void)
{
    const ptt_settings_t defaults[] = {
        kFactory,
        {.modifiers = PTT_MOD_LCTRL | PTT_MOD_RALT, .keys = {0x35, 0x2C}, .max_hold_s = 0},
        {.modifiers = PTT_MOD_LGUI, .keys = {0}, .max_hold_s = 3600},
        {.modifiers = 0, .keys = {0x64, 0x04, 0x05, 0x06, 0x07, 0x08}, .max_hold_s = 1},
    };
    for (size_t i = 0; i < sizeof(defaults) / sizeof(defaults[0]); i++) {
        char text[2048];
        const size_t n = ptt_cfgfile_format_default(&defaults[i], text, sizeof(text));
        CHECK(n < sizeof(text), "default file truncated");

        ptt_settings_t s;
        ptt_cfgfile_result_t res;
        // Parse against different "factory" values so a silent fallback would show
        const ptt_settings_t other = {.modifiers = PTT_MOD_RSHIFT, .keys = {0x3A}, .max_hold_s = defaults[i].max_hold_s};
        ptt_cfgfile_parse(text, n, &other, &s, &res);
        CHECK(!res.chord_error && res.warning_count == 0 && res.diag_count == 0,
              "default file %zu has diagnostics:\n%s", i, text);
        CHECK(memcmp(&s, &defaults[i], sizeof(s)) == 0, "default file %zu does not give back its settings:\n%s", i, text);
    }
}

int main(void)
{
    test_chords();
    test_every_usage_round_trips();
    test_format_truncation();
    test_files();
    test_oversize_file();
    test_many_diags();
    test_status_notes();
    test_default_file();

    printf("%d checks, %d failed\n", s_checks, s_failures);
    return s_failures ? 1 : 0;
}
