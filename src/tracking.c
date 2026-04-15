#include "tracking.h"

#define TAG "TRACKER"

// ---- GLOBALS ----
static tracker_state_t g_track = {0};

static pid_t pid_pan, pid_tilt;
static float current_pan_deg  = 0.0f;
static float current_tilt_deg = 0.0f;

void tracker_init(void) {

}

void tracker_task(void *pv) {

}