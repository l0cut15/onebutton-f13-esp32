#include <stdint.h>
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "ptt_mode.h"

#define CONFIG_FLAG_MAGIC   0x434F4E46u     // "CONF"
// Long enough for the host to see the detach before the device reappears
#define DETACH_MS           100

static const char *TAG = "ptt_mode";

// Not cleared by the startup code. Random after power-on, hence the magic word
// and the reset reason check.
static RTC_NOINIT_ATTR uint32_t s_config_flag;

ptt_mode_t ptt_mode_at_boot(void)
{
    const bool requested = s_config_flag == CONFIG_FLAG_MAGIC && esp_reset_reason() == ESP_RST_SW;
    // One flag gives exactly one config-mode boot
    s_config_flag = 0;
    return requested ? PTT_MODE_CONFIG : PTT_MODE_NORMAL;
}

__attribute__((noreturn)) static void restart(void)
{
    // esp_restart() on the S2 resets the CPU but not the USB peripheral, so without
    // this the host would not see the device leave and come back with new descriptors.
    // (It is a CPU reset, so the strapping pins are not re-sampled: holding BOOT
    // through it does not enter download mode.)
    tud_disconnect();
    vTaskDelay(pdMS_TO_TICKS(DETACH_MS));
    esp_restart();
}

void ptt_mode_enter_config(void)
{
    ESP_LOGI(TAG, "restarting into config mode");
    s_config_flag = CONFIG_FLAG_MAGIC;
    restart();
}

void ptt_mode_enter_normal(void)
{
    ESP_LOGI(TAG, "restarting into normal mode");
    s_config_flag = 0;
    restart();
}
