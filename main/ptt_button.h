/*
 * Button GPIO + debounce, for the main key and the config (BOOT) button.
 *
 * The ISR only posts the button's edge event. The owning task calls
 * ptt_button_on_edge() for each one and ptt_button_update() on every wake-up; an
 * edge is accepted once the pin has been quiet for CONFIG_PTT_DEBOUNCE_MS and its
 * level differs from the last accepted state. A button held at init is not a
 * press: it has to be released and pressed again.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    PTT_BUTTON_KEY,         // CONFIG_PTT_BUTTON_GPIO, posts PTT_EVT_BUTTON_EDGE
    PTT_BUTTON_CONFIG,      // CONFIG_PTT_CONFIG_BUTTON_GPIO, posts PTT_EVT_CONFIG_BUTTON_EDGE
} ptt_button_id_t;

esp_err_t ptt_button_init(ptt_button_id_t id, QueueHandle_t evt_queue);

/** Restart the debounce window (call for every edge event of this button). */
void ptt_button_on_edge(ptt_button_id_t id);

/**
 * Advance the debouncer.
 *
 * @param[out] pressed New debounced state, valid only when true is returned.
 * @return true if a debounced state change was accepted.
 */
bool ptt_button_update(ptt_button_id_t id, bool *pressed);

/**
 * True once per press, when the button has been held (debounced) for
 * CONFIG_PTT_CONFIG_HOLD_MS. Call after ptt_button_update().
 */
bool ptt_button_hold_reached(ptt_button_id_t id);

/** Ticks until this button needs ptt_button_update() again, or portMAX_DELAY if idle. */
TickType_t ptt_button_ticks_to_wait(ptt_button_id_t id);
