#include <assert.h>
#include <stdio.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "sdkconfig.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include "ptt_event.h"
#include "ptt_hid.h"

#define SEND_TIMEOUT_TICKS  pdMS_TO_TICKS(50)
#define WAKE_TIMEOUT_TICKS  pdMS_TO_TICKS(1000)
#define HID_EP_IN           0x81
#define HID_EP_SIZE         8
#define HID_POLL_MS         1
#define HID_COUNTRY_US      33

#if CONFIG_PTT_REMOTE_WAKEUP
#define CONFIG_ATTRIBUTES   TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP
#else
#define CONFIG_ATTRIBUTES   0
#endif

// esp_tinyusb truncates string descriptors to 31 characters
static_assert(sizeof(CONFIG_PTT_USB_MANUFACTURER) - 1 <= 31, "PTT_USB_MANUFACTURER is longer than 31 characters");
static_assert(sizeof(CONFIG_PTT_USB_PRODUCT) - 1 <= 31, "PTT_USB_PRODUCT is longer than 31 characters");

static const char *TAG = "ptt_hid";

static QueueHandle_t s_queue;

/************* Descriptors ****************/

// Boot-compatible keyboard, no report ID: 8-byte reports in both boot and report protocol
static const uint8_t s_report_desc[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

static const tusb_desc_device_t s_device_desc = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,       // class is defined per interface
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = CONFIG_PTT_USB_VID,
    .idProduct = CONFIG_PTT_USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const uint8_t s_config_desc[] = {
    // Configuration number, interface count, string index, total length, attributes, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN, CONFIG_ATTRIBUTES, 100),

    // Same bytes as TUD_HID_DESCRIPTOR(), which hardcodes bCountryCode = 0
    // Interface: boot keyboard, one IN endpoint
    9, TUSB_DESC_INTERFACE, 0, 0, 1, TUSB_CLASS_HID, HID_SUBCLASS_BOOT, HID_ITF_PROTOCOL_KEYBOARD, 0,
    // HID class descriptor: bcdHID 1.11, bCountryCode, 1 report descriptor
    9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), HID_COUNTRY_US, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(s_report_desc)),
    // Endpoint IN
    7, TUSB_DESC_ENDPOINT, HID_EP_IN, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(HID_EP_SIZE), HID_POLL_MS,
};
static_assert(sizeof(s_config_desc) == TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN, "config descriptor length mismatch");

static char s_serial[13];   // 12 hex digits of the factory MAC

static const char *s_string_desc[] = {
    (const char[]){0x09, 0x04},     // 0: supported language is English (0x0409)
    CONFIG_PTT_USB_MANUFACTURER,    // 1: Manufacturer
    CONFIG_PTT_USB_PRODUCT,         // 2: Product
    s_serial,                       // 3: Serial
};

/********* TinyUSB HID callbacks ***************/

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_report_desc;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;   // STALL: no polled reports
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    // Caps/Num Lock LED output reports are ignored
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

/********* esp_tinyusb device events ***************/

// Runs in the TinyUSB task: forward only, never block or send reports here
static void usb_event_cb(tinyusb_event_t *event, void *arg)
{
    (void)arg;
    ptt_event_t evt;

    switch (event->id) {
    case TINYUSB_EVENT_ATTACHED:
        evt = PTT_EVT_USB_MOUNTED;
        break;
    case TINYUSB_EVENT_DETACHED:
        evt = PTT_EVT_USB_UNMOUNTED;
        break;
#ifdef CONFIG_TINYUSB_SUSPEND_CALLBACK
    case TINYUSB_EVENT_SUSPENDED:
        evt = PTT_EVT_USB_SUSPENDED;
        break;
#endif
#ifdef CONFIG_TINYUSB_RESUME_CALLBACK
    case TINYUSB_EVENT_RESUMED:
        evt = PTT_EVT_USB_RESUMED;
        break;
#endif
    default:
        return;
    }
    if (xQueueSend(s_queue, &evt, 0) != pdTRUE) {
        // The PTT task also polls tud_mounted()/tud_suspended(), so this is not fatal
        ESP_LOGW(TAG, "event queue full, dropped USB event %d", (int)evt);
    }
}

/********* API ***************/

esp_err_t ptt_hid_init(QueueHandle_t evt_queue)
{
    s_queue = evt_queue;

    uint8_t mac[6];
    ESP_RETURN_ON_ERROR(esp_efuse_mac_get_default(mac), TAG, "read MAC");
    snprintf(s_serial, sizeof(s_serial), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(usb_event_cb, NULL);
    tusb_cfg.descriptor.device = &s_device_desc;
    tusb_cfg.descriptor.full_speed_config = s_config_desc;
    tusb_cfg.descriptor.string = s_string_desc;
    tusb_cfg.descriptor.string_count = sizeof(s_string_desc) / sizeof(s_string_desc[0]);

    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "tinyusb_driver_install");
    return ESP_OK;
}

bool ptt_hid_host_ready(void)
{
    return tud_mounted() && !tud_suspended();
}

bool ptt_hid_host_suspended(void)
{
    return tud_mounted() && tud_suspended();
}

bool ptt_hid_send(uint8_t modifiers, const uint8_t keys[6])
{
    const TickType_t start = xTaskGetTickCount();

    for (;;) {
        if (!ptt_hid_host_ready()) {
            return false;
        }
        if (tud_hid_ready()) {
            if (tud_hid_keyboard_report(0, modifiers, keys)) {
                return true;
            }
        }
        if (xTaskGetTickCount() - start >= SEND_TIMEOUT_TICKS) {
            ESP_LOGW(TAG, "HID endpoint busy, report dropped (mod=0x%02x)", modifiers);
            return false;
        }
        vTaskDelay(1);
    }
}

bool ptt_hid_wake_host(void)
{
    esp_err_t err = tinyusb_remote_wakeup();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "remote wakeup failed: %s", esp_err_to_name(err));
        return false;
    }

    const TickType_t start = xTaskGetTickCount();
    while (!ptt_hid_host_ready()) {
        if (!tud_mounted() || xTaskGetTickCount() - start >= WAKE_TIMEOUT_TICKS) {
            ESP_LOGW(TAG, "host did not resume after remote wakeup");
            return false;
        }
        vTaskDelay(1);
    }
    return true;
}
