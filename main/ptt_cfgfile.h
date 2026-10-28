/*
 * CONFIG.TXT parser and the text of CONFIG.TXT (default) and STATUS.TXT.
 *
 * Text in, settings and diagnostics out. No I/O and no ESP-IDF dependency, so
 * it builds on the host for unit tests (test/).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ptt_keynames.h"

#define PTT_CFGFILE_MAX_SIZE    4096    // longer files are rejected
#define PTT_CFGFILE_MAX_DIAGS   8       // diagnostics kept; the rest are only counted
#define PTT_MAX_HOLD_S_LIMIT    3600

typedef struct {
    uint8_t modifiers;                      // HID modifier byte
    uint8_t keys[PTT_CHORD_MAX_KEYS];       // usage IDs, listed first and padded with 0
    uint16_t max_hold_s;                    // 0 = unlimited
} ptt_settings_t;

typedef struct {
    unsigned line;                          // 1-based, 0 = not tied to a line
    char msg[80];
} ptt_diag_t;

typedef struct {
    bool chord_error;                       // chord missing/invalid or file unusable: factory chord in use
    size_t error_count;
    size_t warning_count;
    size_t diag_count;                      // diagnostics stored in diags[] (errors and warnings, in file order)
    ptt_diag_t diags[PTT_CFGFILE_MAX_DIAGS];
} ptt_cfgfile_result_t;

typedef struct {
    const char *product;                    // first line of STATUS.TXT: "<product> status"
    const char *version;                    // firmware version
    bool reformatted;                       // storage had to be formatted this boot
    bool wrote_default;                     // CONFIG.TXT was missing and has been written
} ptt_status_info_t;

/**
 * Parse CONFIG.TXT.
 *
 * @param text     File contents (need not be NUL-terminated), or NULL if the file could not be read
 * @param len      Length in bytes. Above PTT_CFGFILE_MAX_SIZE the whole file is rejected.
 * @param defaults Factory settings, used for anything missing or invalid
 * @param[out] out Settings to use
 * @param[out] res Diagnostics
 */
void ptt_cfgfile_parse(const char *text, size_t len, const ptt_settings_t *defaults,
                       ptt_settings_t *out, ptt_cfgfile_result_t *res);

/** Default CONFIG.TXT for the given settings. Returns length (snprintf semantics). */
size_t ptt_cfgfile_format_default(const ptt_settings_t *defaults, char *buf, size_t size);

/** STATUS.TXT for a parse result. Returns length (snprintf semantics). */
size_t ptt_cfgfile_format_status(const ptt_settings_t *settings, const ptt_cfgfile_result_t *res,
                                 const ptt_status_info_t *info, char *buf, size_t size);
