/*
 * One-button USB HID push-to-talk controller (ESP32-S2 mini).
 *
 * Hold the button: the configured chord is held. Release: the chord is released.
 * A single task owns the state machine; the GPIO ISR and USB callbacks only post
 * events to its queue.
 */
#include <assert.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "ptt_button.h"
#include "ptt_config.h"
#include "ptt_event.h"
#include "ptt_hid.h"

#define EVT_QUEUE_LEN       32
#define PTT_TASK_STACK      3072
#define PTT_TASK_PRIO       6
// Upper bound on any wait, so lost events cannot leave the state machine stale
#define WATCHDOG_TICKS      pdMS_TO_TICKS(50)
#define MAX_HOLD_TICKS      pdMS_TO_TICKS((uint32_t)CONFIG_PTT_MAX_HOLD_S * 1000u)

#define LED_ENABLED         (CONFIG_PTT_LED_GPIO >= 0)

#if LED_ENABLED
static_assert(CONFIG_PTT_LED_GPIO != 0, "PTT_LED_GPIO: GPIO0 is the boot strap pin");
static_assert(CONFIG_PTT_LED_GPIO != 19 && CONFIG_PTT_LED_GPIO != 20, "PTT_LED_GPIO: GPIO19/20 are USB D-/D+");
static_assert(CONFIG_PTT_LED_GPIO < 22 || CONFIG_PTT_LED_GPIO > 32,
              "PTT_LED_GPIO: GPIO22-25 do not exist, GPIO26-32 are used by flash/PSRAM");
static_assert(CONFIG_PTT_LED_GPIO != CONFIG_PTT_BUTTON_GPIO, "PTT_LED_GPIO and PTT_BUTTON_GPIO are the same pin");
#endif

typedef enum {
    PTT_IDLE,
    PTT_HELD,
} ptt_state_t;

static const char *TAG = "ptt";

static const uint8_t s_keys[6] = PTT_KEYS;
static const uint8_t s_no_keys[6] = {0};

static QueueHandle_t s_queue;
static ptt_state_t s_state = PTT_IDLE;
static TickType_t s_held_since;

static void led_set(bool on)
{
#if LED_ENABLED
    gpio_set_level((gpio_num_t)CONFIG_PTT_LED_GPIO, on ? 1 : 0);
#else
    (void)on;
#endif
}

static esp_err_t led_init(void)
{
#if LED_ENABLED
    const gpio_config_t cfg = {
        .pin_bit_mask = BIT64(CONFIG_PTT_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err == ESP_OK) {
        led_set(false);
    }
    return err;
#else
    return ESP_OK;
#endif
}

static void chord_press(void)
{
    if (PTT_MODIFIERS != 0) {
        ptt_hid_send(PTT_MODIFIERS, s_no_keys);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_PTT_MOD_TO_KEY_DELAY_MS));
    }
    ptt_hid_send(PTT_MODIFIERS, s_keys);
    led_set(true);
    s_state = PTT_HELD;
    s_held_since = xTaskGetTickCount();
    ESP_LOGI(TAG, "chord down");
}

// Every exit from HELD goes through here and ends with an all-zero report.
// If the host is gone the sends fail fast; the zero report on next mount covers it.
static void chord_release(const char *reason)
{
    if (PTT_MODIFIERS != 0) {
        if (ptt_hid_send(PTT_MODIFIERS, s_no_keys)) {
            vTaskDelay(pdMS_TO_TICKS(CONFIG_PTT_KEY_TO_MOD_DELAY_MS));
        }
    }
    ptt_hid_send(0, s_no_keys);
    led_set(false);
    s_state = PTT_IDLE;
    ESP_LOGI(TAG, "chord up (%s)", reason);
}

static void on_debounced_press(void)
{
    if (ptt_hid_host_ready()) {
        chord_press();
        return;
    }
#if CONFIG_PTT_REMOTE_WAKEUP
    if (ptt_hid_host_suspended()) {
        ESP_LOGI(TAG, "host suspended, requesting remote wakeup");
        if (ptt_hid_wake_host()) {
            chord_press();
        }
        return;
    }
#endif
    // Not mounted (or suspended without remote wakeup): ignore, never queue
    ESP_LOGI(TAG, "press ignored, host not ready");
}

static void handle_event(ptt_event_t evt)
{
    switch (evt) {
    case PTT_EVT_BUTTON_EDGE:
        ptt_button_on_edge();
        break;
    case PTT_EVT_USB_MOUNTED:
        // Clear anything the host may think is held from a previous session.
        // A button already held now stays ignored until it is released and pressed again.
        ESP_LOGI(TAG, "USB mounted");
        if (s_state == PTT_HELD) {
            chord_release("remount");
        } else {
            ptt_hid_send(0, s_no_keys);
        }
        break;
    case PTT_EVT_USB_UNMOUNTED:
    case PTT_EVT_USB_SUSPENDED:
        ESP_LOGI(TAG, "USB %s", evt == PTT_EVT_USB_UNMOUNTED ? "unmounted" : "suspended");
        if (s_state == PTT_HELD) {
            chord_release("usb");
        }
        break;
    case PTT_EVT_USB_RESUMED:
        ESP_LOGI(TAG, "USB resumed");
        break;
    }
}

static TickType_t ticks_to_wait(void)
{
    TickType_t wait = WATCHDOG_TICKS;

    const TickType_t button = ptt_button_ticks_to_wait();
    if (button < wait) {
        wait = button;
    }
    if (s_state == PTT_HELD && CONFIG_PTT_MAX_HOLD_S > 0) {
        const TickType_t held = xTaskGetTickCount() - s_held_since;
        const TickType_t left = held >= MAX_HOLD_TICKS ? 0 : MAX_HOLD_TICKS - held;
        if (left < wait) {
            wait = left;
        }
    }
    return wait;
}

static void ptt_task(void *arg)
{
    (void)arg;

    for (;;) {
        ptt_event_t evt;
        if (xQueueReceive(s_queue, &evt, ticks_to_wait()) == pdTRUE) {
            handle_event(evt);
        }

        // Poll as well as react: covers a USB event dropped by a full queue
        if (s_state == PTT_HELD && !ptt_hid_host_ready()) {
            chord_release("usb lost");
        }

        bool pressed;
        if (ptt_button_update(&pressed)) {
            if (pressed && s_state == PTT_IDLE) {
                on_debounced_press();
            } else if (!pressed && s_state == PTT_HELD) {
                chord_release("button");
            }
        }

        if (s_state == PTT_HELD && CONFIG_PTT_MAX_HOLD_S > 0 &&
                xTaskGetTickCount() - s_held_since >= MAX_HOLD_TICKS) {
            ESP_LOGW(TAG, "max hold time reached, force release");
            chord_release("timeout");
        }
    }
}

void app_main(void)
{
    s_queue = xQueueCreate(EVT_QUEUE_LEN, sizeof(ptt_event_t));
    assert(s_queue);

    ESP_ERROR_CHECK(led_init());
    ESP_ERROR_CHECK(ptt_button_init(s_queue));
    ESP_ERROR_CHECK(ptt_hid_init(s_queue));

    xTaskCreate(ptt_task, "ptt", PTT_TASK_STACK, NULL, PTT_TASK_PRIO, NULL);
}
