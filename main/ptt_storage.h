/*
 * The "storage" FAT partition holding CONFIG.TXT and STATUS.TXT, as seen by the
 * firmware in NORMAL mode. In CONFIG mode the volume belongs to the USB host
 * (ptt_msc.c) and nothing here touches it.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "ptt_cfgfile.h"

#define PTT_STORAGE_PARTITION   "storage"

/**
 * The NORMAL-mode boot sequence: mount the FAT (formatting it if it will not
 * mount), write a default CONFIG.TXT if there is none, parse CONFIG.TXT, update
 * STATUS.TXT if its text changed, unmount.
 *
 * @param factory           Factory default settings (ptt_config.h + Kconfig)
 * @param[out] out          Settings to use. Always valid, even on error.
 * @param[out] chord_error  True if the factory chord is in use because of a problem
 * @return ESP_OK, or the storage error (out is then the factory settings)
 */
esp_err_t ptt_storage_load(const ptt_settings_t *factory, ptt_settings_t *out, bool *chord_error);
