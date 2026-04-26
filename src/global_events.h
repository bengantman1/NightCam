#ifndef GLOBAL_EVENTS_H
#define GLOBAL_EVENTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Make the handles visible to any file that includes this header
extern EventGroupHandle_t event_group;
extern QueueHandle_t frame_queue;

// Define event bits
#define CAMERA_ACTIVE            BIT0
#define WIFI_ACTIVE              BIT1
#define WIFI_CLIENT_DISCONNECTED BIT5

// Frame buffer type needed by record task (producer) and tracking task (consumer) 
typedef struct {
    uint8_t *data; // pointer to pixel array
    size_t   len; // num of bytes in the array
} frame_buf_t;

#endif