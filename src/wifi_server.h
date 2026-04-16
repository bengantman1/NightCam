#pragma once

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "global_events.h"

#include "camera.h"   // for event_group, CAMERA_ACTIVE, RECORDING_DONE, MOUNT_POINT

// ---------------------------------------------------------------------------
// AP configuration — change as needed
// ---------------------------------------------------------------------------
#define AP_SSID         "NightCam-AP"
#define AP_PASSWORD     "nightcam1234"   // min 8 chars; set to "" for open network
#define AP_CHANNEL      1
#define AP_MAX_CONN     2                // max simultaneous stations

// ---------------------------------------------------------------------------
// Server tuning
// ---------------------------------------------------------------------------
#define SERVER_PORT             80
#define FILE_CHUNK_SIZE         (8 * 1024)   // bytes sent per HTTP chunk
#define MAX_URI_LEN             256

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * @brief  FreeRTOS task: starts the WiFi AP and HTTP server whenever the
 *         camera is idle, shuts them down while recording is in progress,
 *         then restarts once RECORDING_DONE is signalled.
 *
 * Stack recommendation: 8192 bytes, priority 3 (below record_task).
 */
void wifi_server_task(void *pv);