#ifndef GLOBAL_EVENTS_H
#define GLOBAL_EVENTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Make the handles visible to any file that includes this header
extern EventGroupHandle_t event_group;
extern QueueHandle_t frame_queue;

// Define event bits
#define PIR_ACTIVATED      BIT0
#define ENVIRONMENT_READY  BIT1
#define CAMERA_ACTIVE      BIT2
#define WIFI_ACTIVE        BIT3
#define RECORDING_DONE     BIT4
#define WIFI_CLIENT_DISCONNECTED BIT5

// define global types
typedef struct {
    uint8_t *data; // pointer to pixel array
    size_t   len; // num of bytes in the array
} frame_buf_t;

#endif