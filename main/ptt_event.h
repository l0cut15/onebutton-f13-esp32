/*
 * Events posted to the PTT task queue by the GPIO ISR and the USB event callback.
 */
#pragma once

typedef enum {
    PTT_EVT_BUTTON_EDGE,    // GPIO ISR saw an edge (not debounced)
    PTT_EVT_USB_MOUNTED,
    PTT_EVT_USB_UNMOUNTED,
    PTT_EVT_USB_SUSPENDED,
    PTT_EVT_USB_RESUMED,
} ptt_event_t;
