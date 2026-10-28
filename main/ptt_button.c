#include <assert.h>
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "ptt_button.h"
#include "ptt_event.h"

#define DEBOUNCE_TICKS  pdMS_TO_TICKS(CONFIG_PTT_DEBOUNCE_MS)
#define HOLD_TICKS      pdMS_TO_TICKS(CONFIG_PTT_CONFIG_HOLD_MS)

static_assert(CONFIG_PTT_BUTTON_GPIO != 0, "PTT_BUTTON_GPIO: GPIO0 is the boot strap pin");
static_assert(CONFIG_PTT_BUTTON_GPIO != 19 && CONFIG_PTT_BUTTON_GPIO != 20,
              "PTT_BUTTON_GPIO: GPIO19/20 are USB D-/D+");
static_assert(CONFIG_PTT_BUTTON_GPIO < 26 || CONFIG_PTT_BUTTON_GPIO > 32,
              "PTT_BUTTON_GPIO: GPIO26-32 are used by in-package flash/PSRAM");
static_assert(CONFIG_PTT_BUTTON_GPIO != 22 && CONFIG_PTT_BUTTON_GPIO != 23 &&
              CONFIG_PTT_BUTTON_GPIO != 24 && CONFIG_PTT_BUTTON_GPIO != 25,
              "PTT_BUTTON_GPIO: GPIO22-25 do not exist on the ESP32-S2");
static_assert(DEBOUNCE_TICKS > 0, "PTT_DEBOUNCE_MS is shorter than one FreeRTOS tick");

// GPIO0 is allowed here: it is the onboard BOOT button, read only after boot
#if CONFIG_PTT_CONFIG_BUTTON_GPIO >= 0
static_assert(CONFIG_PTT_CONFIG_BUTTON_GPIO != 19 && CONFIG_PTT_CONFIG_BUTTON_GPIO != 20,
              "PTT_CONFIG_BUTTON_GPIO: GPIO19/20 are USB D-/D+");
static_assert(CONFIG_PTT_CONFIG_BUTTON_GPIO < 22 || CONFIG_PTT_CONFIG_BUTTON_GPIO > 32,
              "PTT_CONFIG_BUTTON_GPIO: GPIO22-25 do not exist, GPIO26-32 are used by flash/PSRAM");
static_assert(CONFIG_PTT_CONFIG_BUTTON_GPIO != CONFIG_PTT_BUTTON_GPIO,
              "PTT_CONFIG_BUTTON_GPIO and PTT_BUTTON_GPIO are the same pin");
#endif

typedef struct {
    gpio_num_t gpio;
    bool active_low;
    ptt_event_t edge_evt;
    bool stable_pressed;    // last accepted (debounced) state
    bool pending;           // an edge was seen and the pin has not settled yet
    TickType_t last_edge;
    TickType_t pressed_since;
    bool hold_reported;
} button_t;

static const char *TAG = "ptt_button";

static QueueHandle_t s_queue;
static button_t s_buttons[] = {
    [PTT_BUTTON_KEY] = {
        .gpio = (gpio_num_t)CONFIG_PTT_BUTTON_GPIO,
#if CONFIG_PTT_BUTTON_ACTIVE_LOW
        .active_low = true,
#endif
        .edge_evt = PTT_EVT_BUTTON_EDGE,
    },
    [PTT_BUTTON_CONFIG] = {
        .gpio = (gpio_num_t)CONFIG_PTT_CONFIG_BUTTON_GPIO,
        .active_low = true,
        .edge_evt = PTT_EVT_CONFIG_BUTTON_EDGE,
    },
};

static void IRAM_ATTR button_isr(void *arg)
{
    const button_t *b = arg;
    BaseType_t woken = pdFALSE;
    // A full queue drops the edge; ptt_button_update() resamples the pin anyway.
    xQueueSendFromISR(s_queue, &b->edge_evt, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

static bool raw_pressed(const button_t *b)
{
    const int level = gpio_get_level(b->gpio);
    return b->active_low ? level == 0 : level != 0;
}

esp_err_t ptt_button_init(ptt_button_id_t id, QueueHandle_t evt_queue)
{
    button_t *b = &s_buttons[id];
    ESP_RETURN_ON_FALSE(b->gpio >= 0, ESP_ERR_NOT_SUPPORTED, TAG, "button %d disabled", (int)id);
    s_queue = evt_queue;

    const gpio_config_t cfg = {
        .pin_bit_mask = BIT64(b->gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = b->active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = b->active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio_config");

    // Start from the current level: a button held at boot is not a press.
    b->stable_pressed = raw_pressed(b);
    b->pending = false;
    b->hold_reported = true;

    // Shared by both buttons: the second call reports it is already installed
    const esp_err_t err = gpio_install_isr_service(0);
    ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG, "gpio_install_isr_service");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(b->gpio, button_isr, b), TAG, "gpio_isr_handler_add");
    return ESP_OK;
}

void ptt_button_on_edge(ptt_button_id_t id)
{
    button_t *b = &s_buttons[id];
    b->pending = true;
    b->last_edge = xTaskGetTickCount();
}

bool ptt_button_update(ptt_button_id_t id, bool *pressed)
{
    button_t *b = &s_buttons[id];
    const bool raw = raw_pressed(b);

    if (!b->pending) {
        // Safety net for an edge lost to a full queue.
        if (raw != b->stable_pressed) {
            ptt_button_on_edge(id);
        }
        return false;
    }
    if (xTaskGetTickCount() - b->last_edge < DEBOUNCE_TICKS) {
        return false;
    }

    b->pending = false;
    if (raw == b->stable_pressed) {
        return false;   // bounced back to where it was
    }
    b->stable_pressed = raw;
    if (raw) {
        b->pressed_since = xTaskGetTickCount();
        b->hold_reported = false;
    }
    *pressed = raw;
    return true;
}

bool ptt_button_hold_reached(ptt_button_id_t id)
{
    button_t *b = &s_buttons[id];
    if (!b->stable_pressed || b->hold_reported || xTaskGetTickCount() - b->pressed_since < HOLD_TICKS) {
        return false;
    }
    b->hold_reported = true;
    return true;
}

TickType_t ptt_button_ticks_to_wait(ptt_button_id_t id)
{
    const button_t *b = &s_buttons[id];
    const TickType_t now = xTaskGetTickCount();

    if (b->pending) {
        const TickType_t elapsed = now - b->last_edge;
        return elapsed >= DEBOUNCE_TICKS ? 0 : DEBOUNCE_TICKS - elapsed;
    }
    if (b->stable_pressed && !b->hold_reported) {
        const TickType_t held = now - b->pressed_since;
        return held >= HOLD_TICKS ? 0 : HOLD_TICKS - held;
    }
    return portMAX_DELAY;
}
