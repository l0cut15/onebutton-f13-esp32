/*
 * CONFIG mode USB device: one mass storage LUN exposing the "storage" FAT
 * partition to the host.
 *
 * When the host ejects the drive (or goes away), PTT_EVT_MSC_RELEASED is posted
 * to the event queue. The caller then restarts into NORMAL mode.
 */
#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t ptt_msc_init(QueueHandle_t evt_queue);
