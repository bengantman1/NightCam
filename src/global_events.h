#ifndef GLOBAL_EVENTS_H
#define GLOBAL_EVENTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Declaration: Makes the handle visible to any file that includes this header
extern EventGroupHandle_t event_group;

// Define your bit constants here so they are consistent across files
#define PIR_ACTIVATED      BIT0
#define ENVIRONMENT_READY  BIT1
#define CAMERA_ACTIVE      BIT2
#define WIFI_ACTIVE        BIT3

#endif