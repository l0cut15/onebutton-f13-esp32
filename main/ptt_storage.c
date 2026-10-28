#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "diskio_wl.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "sdkconfig.h"
#include "wear_levelling.h"
#include "ptt_storage.h"

#define BASE_PATH       "/cfg"
#define CONFIG_PATH     BASE_PATH "/CONFIG.TXT"
#define STATUS_PATH     BASE_PATH "/STATUS.TXT"
#define VOLUME_LABEL    "ONEBUTTON"
#define TEXT_BUF_SIZE   2048        // default CONFIG.TXT and STATUS.TXT both fit easily

static const char *TAG = "ptt_storage";

// Leaves *reformatted set if the volume had to be (re)created.
static esp_err_t mount(wl_handle_t *wl, bool *reformatted)
{
    esp_vfs_fat_mount_config_t cfg = {
        .max_files = 2,
        .format_if_mount_failed = false,
    };
    *wl = WL_INVALID_HANDLE;
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(BASE_PATH, PTT_STORAGE_PARTITION, &cfg, wl);
    if (err == ESP_OK) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "mount failed (%s), formatting", esp_err_to_name(err));
    // A failed FAT mount leaves the wear-levelling layer mounted
    if (*wl != WL_INVALID_HANDLE) {
        wl_unmount(*wl);
    }
    *reformatted = true;

    cfg.format_if_mount_failed = true;
    err = esp_vfs_fat_spiflash_mount_rw_wl(BASE_PATH, PTT_STORAGE_PARTITION, &cfg, wl);
    if (err != ESP_OK) {
        // Last resort: the wear-levelling metadata itself may be bad. Start from blank flash.
        ESP_LOGW(TAG, "format failed (%s), erasing partition", esp_err_to_name(err));
        if (*wl != WL_INVALID_HANDLE) {
            wl_unmount(*wl);
        }
        const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT,
                                                               PTT_STORAGE_PARTITION);
        ESP_RETURN_ON_FALSE(part, ESP_ERR_NOT_FOUND, TAG, "no '%s' partition", PTT_STORAGE_PARTITION);
        ESP_RETURN_ON_ERROR(esp_partition_erase_range(part, 0, part->size), TAG, "erase");
        ESP_RETURN_ON_ERROR(esp_vfs_fat_spiflash_mount_rw_wl(BASE_PATH, PTT_STORAGE_PARTITION, &cfg, wl), TAG, "mount");
    }

    // "<drive>:<label>", drive numbered as esp_vfs_fat does it
    char label[16];
    snprintf(label, sizeof(label), "%c:%s", (char)('0' + ff_diskio_get_pdrv_wl(*wl)), VOLUME_LABEL);
    if (f_setlabel(label) != FR_OK) {
        ESP_LOGW(TAG, "could not set volume label");
    }
    return ESP_OK;
}

static bool write_file(const char *path, const char *text, size_t len)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "cannot create %s", path);
        return false;
    }
    const bool ok = fwrite(text, 1, len, f) == len;
    // fclose flushes, so its result counts too
    if (fclose(f) != 0 || !ok) {
        ESP_LOGE(TAG, "cannot write %s", path);
        return false;
    }
    return true;
}

// Reads up to size bytes. Returns -1 if the file cannot be opened or read.
static int read_file(const char *path, char *buf, size_t size)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return -1;
    }
    const size_t n = fread(buf, 1, size, f);
    const bool failed = ferror(f);
    fclose(f);
    return failed ? -1 : (int)n;
}

static void write_default_config(const ptt_settings_t *factory, char *buf, bool *wrote)
{
    struct stat st;
    if (stat(CONFIG_PATH, &st) == 0) {
        return;
    }
    ESP_LOGI(TAG, "CONFIG.TXT missing, writing default");
    const size_t len = ptt_cfgfile_format_default(factory, buf, TEXT_BUF_SIZE);
    *wrote = len < TEXT_BUF_SIZE && write_file(CONFIG_PATH, buf, len);
}

static void parse_config(const ptt_settings_t *factory, ptt_settings_t *out, ptt_cfgfile_result_t *res)
{
    // One byte over the limit, so the parser can tell that the file is too long
    char *text = malloc(PTT_CFGFILE_MAX_SIZE + 1);
    const int len = text ? read_file(CONFIG_PATH, text, PTT_CFGFILE_MAX_SIZE + 1) : -1;
    ptt_cfgfile_parse(len < 0 ? NULL : text, len < 0 ? 0 : (size_t)len, factory, out, res);
    free(text);
}

// Only rewrites STATUS.TXT when the text changes, to save flash wear
static void update_status(const char *status, size_t len, char *buf)
{
    const int old_len = read_file(STATUS_PATH, buf, TEXT_BUF_SIZE);
    if (old_len == (int)len && memcmp(buf, status, len) == 0) {
        return;
    }
    ESP_LOGI(TAG, "updating STATUS.TXT");
    write_file(STATUS_PATH, status, len);
}

esp_err_t ptt_storage_load(const ptt_settings_t *factory, ptt_settings_t *out, bool *chord_error)
{
    *out = *factory;
    *chord_error = true;

    char *status = malloc(TEXT_BUF_SIZE);
    char *scratch = malloc(TEXT_BUF_SIZE);
    ptt_cfgfile_result_t *res = malloc(sizeof(*res));
    esp_err_t err = ESP_ERR_NO_MEM;
    if (status == NULL || scratch == NULL || res == NULL) {
        goto done;
    }

    wl_handle_t wl;
    ptt_status_info_t info = {
        .product = CONFIG_PTT_USB_PRODUCT,
        .version = esp_app_get_description()->version,
    };
    err = mount(&wl, &info.reformatted);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "storage unusable (%s), using factory settings", esp_err_to_name(err));
        goto done;
    }

    write_default_config(factory, scratch, &info.wrote_default);
    parse_config(factory, out, res);
    for (size_t i = 0; i < res->diag_count; i++) {
        ESP_LOGW(TAG, "CONFIG.TXT line %u: %s", res->diags[i].line, res->diags[i].msg);
    }
    *chord_error = res->chord_error;

    const size_t len = ptt_cfgfile_format_status(out, res, &info, status, TEXT_BUF_SIZE);
    if (len < TEXT_BUF_SIZE) {
        update_status(status, len, scratch);
    }

    esp_vfs_fat_spiflash_unmount_rw_wl(BASE_PATH, wl);

done:
    free(res);
    free(scratch);
    free(status);
    return err;
}
