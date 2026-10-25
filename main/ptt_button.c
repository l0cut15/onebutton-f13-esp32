#include <assert.h>
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "ptt_button.h"
#include "ptt_event.h"

#define BUTTON_GPIO     ((gpio_num_t)CONFIG_PTT_BUTTON_GPIO)
#define DEBOUNCE_TICKS  pdMS_TO_TICKS(CONFIG_PTT_DEBOUNCE_MS)

static_assert(CONFIG_PTT_BUTTON_GPIO != 0, "PTT_BUTTON_GPIO: GPIO0 is the boot strap pin");
static_assert(CONFIG_PTT_BUTTON_GPIO != 19 && CONFIG_PTT_BUTTON_GPIO != 20,
              "PTT_BUTTON_GPIO: GPIO19/20 are USB D-/D+");
static_assert(CONFIG_PTT_BUTTON_GPIO < 26 || CONFIG_PTT_BUTTON_GPIO > 32,
              "PTT_BUTTON_GPIO: GPIO26-32 are used by in-package flash/PSRAM");
static_assert(CONFIG_PTT_BUTTON_GPIO != 22 && CONFIG_PTT_BUTTON_GPIO != 23 &&
              CONFIG_PTT_BUTTON_GPIO != 24 && CONFIG_PTT_BUTTON_GPIO != 25,
              "PTT_BUTTON_GPIO: GPIO22-25 do not exist on the ESP32-S2");
static_assert(DEBOUNCE_TICKS > 0, "PTT_DEBOUNCE_MS is shorter than one FreeRTOS tick");

static const char *TAG = "ptt_button";

static QueueHandle_t s_queue;
static bool s_stable_pressed;   // last accepted (debounced) state
static bool s_pending;          // an edge was seen and the pin has not settled yet
static TickType_t s_last_edge;

static void IRAM_ATTR button_isr(void *arg)
{
    (void)arg;
    const ptt_event_t evt = PTT_EVT_BUTTON_EDGE;
    BaseType_t woken = pdFALSE;
    // A full queue drops the edge; ptt_button_update() resamples the pin anyway.
    xQueueSendFromISR(s_queue, &evt, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

static bool raw_pressed(void)
{
    const int level = gpio_get_level(BUTTON_GPIO);
#if CONFIG_PTT_BUTTON_ACTIVE_LOW
    return level == 0;
#else
    return level != 0;
#endif
}

esp_err_t ptt_button_init(QueueHandle_t evt_queue)
{
    s_queue = evt_queue;

    const gpio_config_t cfg = {
        .pin_bit_mask = BIT64(BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
#if CONFIG_PTT_BUTTON_ACTIVE_LOW
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
#else
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
#endif
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio_config");

    // Start from the current level: a button held at boot is not a press.
    s_stable_pressed = raw_pressed();
    s_pending = false;

    ESP_RETURN_ON_ERROR(gpio_install_isr_service(0), TAG, "gpio_install_isr_service");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(BUTTON_GPIO, button_isr, NULL), TAG, "gpio_isr_handler_add");
    return ESP_OK;
}

void ptt_button_on_edge(void)
{
    s_pending = true;
    s_last_edge = xTaskGetTickCount();
}

bool ptt_button_update(bool *pressed)
{
    const bool raw = raw_pressed();

    if (!s_pending) {
        // Safety net for an edge lost to a full queue.
        if (raw != s_stable_pressed) {
            ptt_button_on_edge();
        }
        return false;
    }
    if (xTaskGetTickCount() - s_last_edge < DEBOUNCE_TICKS) {
        return false;
    }

    s_pending = false;
    if (raw == s_stable_pressed) {
        return false;   // bounced back to where it was
    }
    s_stable_pressed = raw;
    *pressed = raw;
    return true;
}

TickType_t ptt_button_ticks_to_wait(void)
{
    if (!s_pending) {
        return portMAX_DELAY;
    }
    const TickType_t elapsed = xTaskGetTickCount() - s_last_edge;
    return elapsed >= DEBOUNCE_TICKS ? 0 : DEBOUNCE_TICKS - elapsed;
}
