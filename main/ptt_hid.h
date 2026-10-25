/*
 * USB device (esp_tinyusb) + boot-keyboard HID report sending.
 *
 * USB mount/unmount/suspend/resume are forwarded to the PTT task queue as
 * ptt_event_t; nothing in here runs the state machine.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t ptt_hid_init(QueueHandle_t evt_queue);

/** Mounted and not suspended: input reports can be delivered. */
bool ptt_hid_host_ready(void);

/** Mounted but the host has suspended the bus. */
bool ptt_hid_host_suspended(void);

/**
 * Send one 8-byte boot keyboard report. Waits a bounded time for the IN endpoint,
 * returns false immediately if the host is not ready. Task context only.
 *
 * @param keys 6 usage IDs, or NULL for none.
 */
bool ptt_hid_send(uint8_t modifiers, const uint8_t keys[6]);

/**
 * Signal remote wakeup and wait (bounded) for the host to resume the bus.
 * @return true if the host is ready afterwards.
 */
bool ptt_hid_wake_host(void);
