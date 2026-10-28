/*
 * NORMAL (HID keyboard) vs CONFIG (USB drive) mode.
 *
 * The mode is chosen once at boot. Switching is always a restart: a magic word in
 * RTC memory survives esp_restart() but not power loss, so unplugging always comes
 * back in NORMAL.
 */
#pragma once

typedef enum {
    PTT_MODE_NORMAL,
    PTT_MODE_CONFIG,
} ptt_mode_t;

/** Read and clear the config flag. Call once, early in app_main(). */
ptt_mode_t ptt_mode_at_boot(void);

/** Detach from USB and restart into CONFIG mode. */
void ptt_mode_enter_config(void) __attribute__((noreturn));

/** Detach from USB and restart into NORMAL mode. */
void ptt_mode_enter_normal(void) __attribute__((noreturn));
