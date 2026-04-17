#pragma once
#include "global_events.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "pid.h"
#include "servo.h"
#include "esp_camera.h"

// Effective resolution after JPG_SCALE_4X (from 320x240)
#define FRAME_WIDTH        80
#define FRAME_HEIGHT       60
#define FRAME_SIZE         (FRAME_WIDTH * FRAME_HEIGHT)

#define FRAME_CENTER_X     (FRAME_WIDTH  / 2)
#define FRAME_CENTER_Y     (FRAME_HEIGHT / 2)

// Motion detection thresholds
#define TRACK_THRESHOLD    15     // Per-pixel diff to count as motion
#define TRACK_MIN_MASS     20     // Min active pixels to declare a target

// Timing
#define TRACK_INTERVAL_MS  66     // ~15 FPS

// Block-scan optimisation
#define BLOCK              8
#define SAMPLE_STEP        4
#define BLOCK_THRESHOLD    40

// Region of interest window (pixels) when locked on a target
#define ROI_RADIUS         32

// FreeRTOS task config — pin to core 1 (PRO_CPU) away from WiFi/BT
#define TRACKER_TASK_STACK   4096
#define TRACKER_TASK_PRIO    5
#define TRACKER_TASK_CORE    1     // PRO_CPU_NUM

// JPEG → grayscale decode callback (provided by your camera module)
typedef struct {
    uint8_t *buf;
    size_t   index;
    size_t   max;
} gray_ctx_t;

typedef struct {
    bool valid;
    int  x;
    int  y;
} motion_result_t;

typedef struct {
    int  last_x;
    int  last_y;
    bool has_lock;
} tracker_state_t;

void tracker_init(void);
void tracker_task(void *pv);


