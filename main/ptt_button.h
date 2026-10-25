/*
 * Button GPIO + debounce.
 *
 * The ISR only posts PTT_EVT_BUTTON_EDGE. The PTT task calls ptt_button_on_edge()
 * for each one and ptt_button_update() on every wake-up; an edge is accepted once
 * the pin has been quiet for CONFIG_PTT_DEBOUNCE_MS and its level differs from the
 * last accepted state.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t ptt_button_init(QueueHandle_t evt_queue);

/** Restart the debounce window (call for every PTT_EVT_BUTTON_EDGE). */
void ptt_button_on_edge(void);

/**
 * Advance the debouncer.
 *
 * @param[out] pressed New debounced state, valid only when true is returned.
 * @return true if a debounced state change was accepted.
 */
bool ptt_button_update(bool *pressed);

/** Ticks until ptt_button_update() needs to run again, or portMAX_DELAY if idle. */
TickType_t ptt_button_ticks_to_wait(void);
