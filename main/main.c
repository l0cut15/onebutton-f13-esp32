/*
 * One-button USB HID push-to-talk controller (ESP32-S2 mini).
 *
 * NORMAL mode: hold the button, the configured chord is held. Release: the chord
 * is released. The chord comes from CONFIG.TXT on the storage partition.
 * CONFIG mode (hold BOOT): the device restarts as a USB drive holding CONFIG.TXT,
 * and restarts as a keyboard again when the drive is ejected.
 *
 * A single task owns each mode's state machine; the GPIO ISR and USB callbacks
 * only post events to its queue.
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
#include "ptt_cfgfile.h"
#include "ptt_config.h"
#include "ptt_event.h"
#include "ptt_hid.h"
#include "ptt_mode.h"
#include "ptt_msc.h"
#include "ptt_storage.h"

#define EVT_QUEUE_LEN       32
#define PTT_TASK_STACK      3072
#define PTT_TASK_PRIO       6
// Upper bound on any wait, so lost events cannot leave the state machine stale
#define WATCHDOG_TICKS      pdMS_TO_TICKS(50)
// Config mode LED: slow blink, 1 s period
#define CONFIG_BLINK_TICKS  pdMS_TO_TICKS(500)
// After eject: let the SCSI status phase finish before dropping off the bus
#define EJECT_SETTLE_TICKS  pdMS_TO_TICKS(200)
// Boot pattern when CONFIG.TXT had an error: 3 short blinks
#define ERROR_BLINKS        3
#define ERROR_BLINK_MS      100

#define LED_ENABLED             (CONFIG_PTT_LED_GPIO >= 0)
#define CONFIG_BUTTON_ENABLED   (CONFIG_PTT_CONFIG_BUTTON_GPIO >= 0)

#if LED_ENABLED
static_assert(CONFIG_PTT_LED_GPIO != 0, "PTT_LED_GPIO: GPIO0 is the boot strap pin");
static_assert(CONFIG_PTT_LED_GPIO != 19 && CONFIG_PTT_LED_GPIO != 20, "PTT_LED_GPIO: GPIO19/20 are USB D-/D+");
static_assert(CONFIG_PTT_LED_GPIO < 22 || CONFIG_PTT_LED_GPIO > 32,
              "PTT_LED_GPIO: GPIO22-25 do not exist, GPIO26-32 are used by flash/PSRAM");
static_assert(CONFIG_PTT_LED_GPIO != CONFIG_PTT_BUTTON_GPIO, "PTT_LED_GPIO and PTT_BUTTON_GPIO are the same pin");
static_assert(CONFIG_PTT_LED_GPIO != CONFIG_PTT_CONFIG_BUTTON_GPIO,
              "PTT_LED_GPIO and PTT_CONFIG_BUTTON_GPIO are the same pin");
#endif

typedef enum {
    PTT_IDLE,
    PTT_HELD,
} ptt_state_t;

static const char *TAG = "ptt";

// ptt_config.h is now the factory default, used when CONFIG.TXT is missing or invalid
static const ptt_settings_t s_factory = {
    .modifiers = PTT_MODIFIERS,
    .keys = PTT_KEYS,
    .max_hold_s = CONFIG_PTT_MAX_HOLD_S,
};
static const uint8_t s_no_keys[6] = {0};

static QueueHandle_t s_queue;
static ptt_settings_t s_settings;
static TickType_t s_max_hold_ticks;     // 0 = unlimited
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

static void led_blink(int count, uint32_t ms)
{
    for (int i = 0; i < count; i++) {
        led_set(true);
        vTaskDelay(pdMS_TO_TICKS(ms));
        led_set(false);
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
}

/************* NORMAL mode: HID keyboard ****************/

static void chord_press(void)
{
    if (s_settings.modifiers != 0) {
        ptt_hid_send(s_settings.modifiers, s_no_keys);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_PTT_MOD_TO_KEY_DELAY_MS));
    }
    ptt_hid_send(s_settings.modifiers, s_settings.keys);
    led_set(true);
    s_state = PTT_HELD;
    s_held_since = xTaskGetTickCount();
    ESP_LOGI(TAG, "chord down");
}

// Every exit from HELD goes through here and ends with an all-zero report.
// If the host is gone the sends fail fast; the zero report on next mount covers it.
static void chord_release(const char *reason)
{
    if (s_settings.modifiers != 0) {
        if (ptt_hid_send(s_settings.modifiers, s_no_keys)) {
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
        ptt_button_on_edge(PTT_BUTTON_KEY);
        break;
    case PTT_EVT_CONFIG_BUTTON_EDGE:
        ptt_button_on_edge(PTT_BUTTON_CONFIG);
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
    case PTT_EVT_MSC_RELEASED:
        break;  // config mode only
    }
}

static TickType_t ticks_to_wait(void)
{
    TickType_t wait = WATCHDOG_TICKS;

    const TickType_t button = ptt_button_ticks_to_wait(PTT_BUTTON_KEY);
    if (button < wait) {
        wait = button;
    }
#if CONFIG_BUTTON_ENABLED
    const TickType_t config_button = ptt_button_ticks_to_wait(PTT_BUTTON_CONFIG);
    if (config_button < wait) {
        wait = config_button;
    }
#endif
    if (s_state == PTT_HELD && s_max_hold_ticks > 0) {
        const TickType_t held = xTaskGetTickCount() - s_held_since;
        const TickType_t left = held >= s_max_hold_ticks ? 0 : s_max_hold_ticks - held;
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
        if (ptt_button_update(PTT_BUTTON_KEY, &pressed)) {
            if (pressed && s_state == PTT_IDLE) {
                on_debounced_press();
            } else if (!pressed && s_state == PTT_HELD) {
                chord_release("button");
            }
        }

        if (s_state == PTT_HELD && s_max_hold_ticks > 0 &&
                xTaskGetTickCount() - s_held_since >= s_max_hold_ticks) {
            ESP_LOGW(TAG, "max hold time reached, force release");
            chord_release("timeout");
        }

#if CONFIG_BUTTON_ENABLED
        ptt_button_update(PTT_BUTTON_CONFIG, &pressed);
        if (ptt_button_hold_reached(PTT_BUTTON_CONFIG)) {
            // Never restart with a key down on the host
            if (s_state == PTT_HELD) {
                chord_release("config mode");
            }
            ptt_mode_enter_config();
        }
#endif
    }
}

/************* CONFIG mode: USB drive ****************/

static void config_task(void *arg)
{
    (void)arg;
    bool led = false;
    TickType_t last_toggle = xTaskGetTickCount();

    for (;;) {
        const TickType_t since = xTaskGetTickCount() - last_toggle;
        TickType_t wait = since >= CONFIG_BLINK_TICKS ? 0 : CONFIG_BLINK_TICKS - since;
#if CONFIG_BUTTON_ENABLED
        const TickType_t button = ptt_button_ticks_to_wait(PTT_BUTTON_CONFIG);
        if (button < wait) {
            wait = button;
        }
#endif

        ptt_event_t evt;
        if (xQueueReceive(s_queue, &evt, wait) == pdTRUE) {
            if (evt == PTT_EVT_CONFIG_BUTTON_EDGE) {
                ptt_button_on_edge(PTT_BUTTON_CONFIG);
            } else if (evt == PTT_EVT_MSC_RELEASED) {
                ESP_LOGI(TAG, "drive ejected");
                vTaskDelay(EJECT_SETTLE_TICKS);
                ptt_mode_enter_normal();
            }
        }

        if (xTaskGetTickCount() - last_toggle >= CONFIG_BLINK_TICKS) {
            led = !led;
            led_set(led);
            last_toggle = xTaskGetTickCount();
        }

#if CONFIG_BUTTON_ENABLED
        bool pressed;
        ptt_button_update(PTT_BUTTON_CONFIG, &pressed);
        if (ptt_button_hold_reached(PTT_BUTTON_CONFIG)) {
            ptt_mode_enter_normal();
        }
#endif
    }
}

/************* Startup ****************/

static void start_config_mode(void)
{
    ESP_LOGI(TAG, "config mode");
    // A failure aborts, and the panic reset comes back in NORMAL mode, which
    // repairs the volume if that was the problem.
    ESP_ERROR_CHECK(ptt_msc_init(s_queue));
#if CONFIG_BUTTON_ENABLED
    ESP_ERROR_CHECK(ptt_button_init(PTT_BUTTON_CONFIG, s_queue));
#endif
    xTaskCreate(config_task, "ptt_cfg", PTT_TASK_STACK, NULL, PTT_TASK_PRIO, NULL);
}

static void start_normal_mode(void)
{
    // Storage is done and unmounted before USB starts: one owner at a time
    bool chord_error;
    if (ptt_storage_load(&s_factory, &s_settings, &chord_error) != ESP_OK) {
        ESP_LOGE(TAG, "storage failed, using factory settings");
    }
    s_max_hold_ticks = pdMS_TO_TICKS((uint32_t)s_settings.max_hold_s * 1000u);

    ESP_ERROR_CHECK(ptt_button_init(PTT_BUTTON_KEY, s_queue));
#if CONFIG_BUTTON_ENABLED
    ESP_ERROR_CHECK(ptt_button_init(PTT_BUTTON_CONFIG, s_queue));
#endif
    ESP_ERROR_CHECK(ptt_hid_init(s_queue));

    // USB enumerates in the background meanwhile; events wait in the queue
    if (chord_error) {
        led_blink(ERROR_BLINKS, ERROR_BLINK_MS);
    }
    xTaskCreate(ptt_task, "ptt", PTT_TASK_STACK, NULL, PTT_TASK_PRIO, NULL);
}

void app_main(void)
{
    const ptt_mode_t mode = ptt_mode_at_boot();

    s_queue = xQueueCreate(EVT_QUEUE_LEN, sizeof(ptt_event_t));
    assert(s_queue);
    ESP_ERROR_CHECK(led_init());

    if (mode == PTT_MODE_CONFIG) {
        start_config_mode();
    } else {
        start_normal_mode();
    }
}
