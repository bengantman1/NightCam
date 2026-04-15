#pragma once
#include "global_events.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "pid.h"

#define FRAME_WIDTH        80     // using JPG_SCALE_4X from 320x240
#define FRAME_HEIGHT       60
#define FRAME_CENTER_X     (FRAME_WIDTH / 2)
#define FRAME_CENTER_Y     (FRAME_HEIGHT / 2)

#define TRACK_THRESHOLD    15
#define TRACK_MIN_MASS     20
#define TRACK_INTERVAL_MS  66     // ~15 FPS

#define BLOCK              8
#define SAMPLE_STEP        4
#define BLOCK_THRESHOLD    40
#define ROI_RADIUS         32

typedef struct {
    bool valid;
    int x;
    int y;
} motion_result_t;

typedef struct {
    int last_x;
    int last_y;
    bool has_lock;
} tracker_state_t;

// grayscale decode callback context
typedef struct {
    uint8_t *buf;
    size_t index;
    size_t max;
} gray_ctx_t;

void tracker_init(void);
void tracker_task(void *pv);


