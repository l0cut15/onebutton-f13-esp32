/*
 * Events posted to the PTT task queue by the GPIO ISR and the USB callbacks.
 */
#pragma once

typedef enum {
    PTT_EVT_BUTTON_EDGE,        // main key GPIO ISR saw an edge (not debounced)
    PTT_EVT_CONFIG_BUTTON_EDGE, // config button GPIO ISR saw an edge (not debounced)
    PTT_EVT_USB_MOUNTED,
    PTT_EVT_USB_UNMOUNTED,
    PTT_EVT_USB_SUSPENDED,
    PTT_EVT_USB_RESUMED,
    PTT_EVT_MSC_RELEASED,       // config mode: the host ejected the drive (or went away)
} ptt_event_t;
