#include "tracking.h"

#define TAG "TRACKER"

static tracker_state_t g_track        = {0};
static PID_t           pid_pan;
static PID_t           pid_tilt;
static float           current_pan_deg  = 0.0f;
static float           current_tilt_deg = 0.0f;

static motion_result_t find_motion_centroid(uint8_t *prev, uint8_t *curr) {

    int x_start = 0, x_end = FRAME_WIDTH;
    int y_start = 0, y_end = FRAME_HEIGHT;

    // Narrow the search window once we have a lock
    if (g_track.has_lock) {
        x_start = g_track.last_x - ROI_RADIUS;
        x_end   = g_track.last_x + ROI_RADIUS;
        y_start = g_track.last_y - ROI_RADIUS;
        y_end   = g_track.last_y + ROI_RADIUS;

        if (x_start < 0)            x_start = 0;
        if (y_start < 0)            y_start = 0;
        if (x_end   > FRAME_WIDTH)  x_end   = FRAME_WIDTH;
        if (y_end   > FRAME_HEIGHT) y_end   = FRAME_HEIGHT;
    }

    int mass  = 0;
    int sum_x = 0;
    int sum_y = 0;

    for (int y = y_start; y < y_end; y += BLOCK) {
        for (int x = x_start; x < x_end; x += BLOCK) {

            // ---- Sparse sample: cheap reject for quiet blocks ----
            int block_diff = 0;

            for (int by = 0; by < BLOCK; by += SAMPLE_STEP) {
                int yy = y + by;
                if (yy >= y_end) break;

                int row = yy * FRAME_WIDTH + x;

                for (int bx = 0; bx < BLOCK; bx += SAMPLE_STEP) {
                    int xx = x + bx;
                    if (xx >= x_end) break;

                    block_diff += abs(prev[row + bx] - curr[row + bx]);
                }
            }

            if (block_diff < BLOCK_THRESHOLD) continue;

            // ---- Full scan: only active blocks reach here ----
            for (int by = 0; by < BLOCK; by++) {
                int yy = y + by;
                if (yy >= y_end) break;

                int row = yy * FRAME_WIDTH + x;

                for (int bx = 0; bx < BLOCK; bx++) {
                    int xx = x + bx;
                    if (xx >= x_end) break;

                    int d = abs(prev[row + bx] - curr[row + bx]);

                    if (d > TRACK_THRESHOLD) {
                        mass++;
                        sum_x += xx;
                        sum_y += yy;
                    }
                }
            }
        }
    }

    motion_result_t res = {0};

    if (mass > TRACK_MIN_MASS) {
        res.valid        = true;
        res.x            = sum_x / mass;
        res.y            = sum_y / mass;
        g_track.last_x   = res.x;
        g_track.last_y   = res.y;
        g_track.has_lock = true;
    } else {
        res.valid        = false;
        g_track.has_lock = false;
    }

    return res;
}

void tracker_init(void) {
    servo_init();

    // Tuned for 80x60 low-res tracking
    pid_init(&pid_pan,  0.15f, 0.002f, 0.02f, -90.0f, 90.0f);
    pid_init(&pid_tilt, 0.15f, 0.002f, 0.02f, -90.0f, 90.0f);

    servo_set_pan(0.0f);
    servo_set_tilt(0.0f);
}

void tracker_task(void *pv) {

    uint8_t *gray_prev = heap_caps_malloc(FRAME_SIZE, MALLOC_CAP_SPIRAM);
    uint8_t *gray_curr = heap_caps_malloc(FRAME_SIZE, MALLOC_CAP_SPIRAM);

    if (!gray_prev || !gray_curr) {
        ESP_LOGE(TAG, "PSRAM alloc failed — need %u bytes x2", FRAME_SIZE);
        vTaskDelete(NULL);
        return;
    }

    camera_fb_t *fb = NULL;

    // ---- Seed the previous frame ----
    if (xQueueReceive(frame_queue, &fb, portMAX_DELAY)) {
        gray_ctx_t ctx = { gray_prev, 0, FRAME_SIZE };
        //esp_jpeg_decode(fb->buf, fb->len, JPG_SCALE_4X, _gray_out_cb, &ctx);
        esp_camera_fb_return(fb);
    }

    const float dt = TRACK_INTERVAL_MS / 1000.0f;

    while (1) {

        TickType_t t_start = xTaskGetTickCount();

        // ---- Drain the queue; process only the freshest frame ----
        if (!xQueueReceive(frame_queue, &fb, portMAX_DELAY)) continue;

        camera_fb_t *latest = fb;

        while (xQueueReceive(frame_queue, &fb, 0)) {
            esp_camera_fb_return(latest);
            latest = fb;
        }

        // ---- Decode at 1/4 resolution for speed ----
        gray_ctx_t ctx = { gray_curr, 0, FRAME_SIZE };
        //esp_jpg_decode(latest->buf, latest->len, JPG_SCALE_4X, _gray_out_cb, &ctx);
        esp_camera_fb_return(latest);

        // ---- Motion detection ----
        motion_result_t motion = find_motion_centroid(gray_prev, gray_curr);

        if (motion.valid) {

            float ex = (float)(motion.x - FRAME_CENTER_X);
            float ey = (float)(motion.y - FRAME_CENTER_Y);

            float pan_delta  =  pid_update(&pid_pan,  ex, dt);
            float tilt_delta = -pid_update(&pid_tilt, ey, dt);   // Y-axis inverted

            current_pan_deg  += pan_delta;
            current_tilt_deg += tilt_delta;

            // Hard clamp to servo limits
            if (current_pan_deg  >  90.0f) current_pan_deg  =  90.0f;
            if (current_pan_deg  < -90.0f) current_pan_deg  = -90.0f;
            if (current_tilt_deg >  45.0f) current_tilt_deg =  45.0f;
            if (current_tilt_deg < -45.0f) current_tilt_deg = -45.0f;

            servo_set_pan(current_pan_deg);
            servo_set_tilt(current_tilt_deg);

            ESP_LOGI(TAG, "Track (%d,%d) → pan=%.1f tilt=%.1f",
                     motion.x, motion.y,
                     current_pan_deg, current_tilt_deg);
        }

        // ---- Swap buffers (no copy — just pointer swap) ----
        uint8_t *tmp = gray_prev;
        gray_prev    = gray_curr;
        gray_curr    = tmp;

        // ---- Rate control: yield remaining slice to scheduler ----
        TickType_t elapsed = xTaskGetTickCount() - t_start;
        TickType_t budget  = pdMS_TO_TICKS(TRACK_INTERVAL_MS);

        if (elapsed < budget) {
            vTaskDelay(budget - elapsed);
        }
    }
}