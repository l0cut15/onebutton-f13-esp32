#include <assert.h>
#include <stdio.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_partition.h"
#include "sdkconfig.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "wear_levelling.h"
#include "ptt_event.h"
#include "ptt_msc.h"
#include "ptt_storage.h"

#define MSC_EP_OUT          0x01
#define MSC_EP_IN           0x81
#define MSC_EP_SIZE         64      // full-speed bulk
#define CONFIG_PRODUCT      CONFIG_PTT_USB_PRODUCT " Config"

// esp_tinyusb truncates string descriptors to 31 characters
static_assert(sizeof(CONFIG_PRODUCT) - 1 <= 31, "PTT_USB_PRODUCT is longer than 24 characters");
static_assert(CONFIG_PTT_USB_PID_CONFIG != CONFIG_PTT_USB_PID,
              "PTT_USB_PID_CONFIG must differ from PTT_USB_PID (hosts cache interfaces per VID/PID)");

static const char *TAG = "ptt_msc";

static QueueHandle_t s_queue;

/************* Descriptors ****************/

// Same as the keyboard except for the PID and product string
static const tusb_desc_device_t s_device_desc = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,       // class is defined per interface
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = CONFIG_PTT_USB_VID,
    .idProduct = CONFIG_PTT_USB_PID_CONFIG,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const uint8_t s_config_desc[] = {
    // Configuration number, interface count, string index, total length, attributes, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN, 0, 100),
    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(0, 0, MSC_EP_OUT, MSC_EP_IN, MSC_EP_SIZE),
};

static char s_serial[13];   // 12 hex digits of the factory MAC, as for the keyboard

static const char *s_string_desc[] = {
    (const char[]){0x09, 0x04},     // 0: supported language is English (0x0409)
    CONFIG_PTT_USB_MANUFACTURER,    // 1: Manufacturer
    CONFIG_PRODUCT,                 // 2: Product
    s_serial,                       // 3: Serial
};

/********* esp_tinyusb storage events ***************/

// esp_tinyusb 2.4.0 implements tud_msc_start_stop_cb() itself. On eject (START STOP
// UNIT, LoEj=1, Start=0) and on USB detach it hands the volume back to the app
// (msc_storage_mount_to_app()), which ends in one of these events. That hand-back
// is the eject signal. Runs in the TinyUSB task, inside the SCSI command: forward
// only, the status phase has not been sent yet.
static void storage_event_cb(tinyusb_msc_storage_handle_t handle, tinyusb_msc_event_t *event, void *arg)
{
    (void)handle;
    (void)arg;

    switch (event->id) {
    case TINYUSB_MSC_EVENT_MOUNT_COMPLETE:
        if (event->mount_point != TINYUSB_MSC_STORAGE_MOUNT_APP) {
            return;
        }
        break;
    case TINYUSB_MSC_EVENT_MOUNT_FAILED:
    case TINYUSB_MSC_EVENT_FORMAT_REQUIRED:
    case TINYUSB_MSC_EVENT_FORMAT_FAILED:
        // The host left the FAT unmountable; the NORMAL boot repairs it
        ESP_LOGW(TAG, "volume did not mount after eject (event %d)", (int)event->id);
        break;
    default:
        return;
    }
    const ptt_event_t evt = PTT_EVT_MSC_RELEASED;
    if (xQueueSend(s_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, eject dropped");
    }
}

/********* API ***************/

esp_err_t ptt_msc_init(QueueHandle_t evt_queue)
{
    s_queue = evt_queue;

    uint8_t mac[6];
    ESP_RETURN_ON_ERROR(esp_efuse_mac_get_default(mac), TAG, "read MAC");
    snprintf(s_serial, sizeof(s_serial), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT,
                                                           PTT_STORAGE_PARTITION);
    ESP_RETURN_ON_FALSE(part, ESP_ERR_NOT_FOUND, TAG, "no '%s' partition", PTT_STORAGE_PARTITION);
    wl_handle_t wl;
    ESP_RETURN_ON_ERROR(wl_mount(part, &wl), TAG, "wl_mount");

    // auto_mount_off stays 0: the automatic hand-back to the app on eject is what
    // raises the event above.
    const tinyusb_msc_driver_config_t msc_cfg = {
        .callback = storage_event_cb,
    };
    ESP_RETURN_ON_ERROR(tinyusb_msc_install_driver(&msc_cfg), TAG, "tinyusb_msc_install_driver");

    const tinyusb_msc_storage_config_t storage_cfg = {
        .medium.wl_handle = wl,
        // The NORMAL boot formats when needed. Never format here: after eject the
        // volume holds whatever the host left, and the user's file must survive.
        .fat_fs.do_not_format = true,
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
    };
    ESP_RETURN_ON_ERROR(tinyusb_msc_new_storage_spiflash(&storage_cfg, NULL), TAG, "tinyusb_msc_new_storage_spiflash");

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = &s_device_desc;
    tusb_cfg.descriptor.full_speed_config = s_config_desc;
    tusb_cfg.descriptor.string = s_string_desc;
    tusb_cfg.descriptor.string_count = sizeof(s_string_desc) / sizeof(s_string_desc[0]);

    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "tinyusb_driver_install");
    return ESP_OK;
}
