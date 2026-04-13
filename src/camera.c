#include "camera.h"

static const char* TAG = "CAMERA"; // Tag for print statements

typedef struct {
    uint8_t *data; // pointer to pixel array
    size_t   len; // num of bytes in the array
} frame_buf_t;

static frame_buf_t frames[MAX_FRAMES];
static bool        psram_ready = false;

esp_err_t camera_init() {
    camera_config_t config = {
        .pin_pwdn     = PWDN_GPIO_NUM,
        .pin_reset    = RESET_GPIO_NUM,
        .pin_xclk     = XCLK_GPIO_NUM,
        .pin_sccb_sda = SIOD_GPIO_NUM,
        .pin_sccb_scl = SIOC_GPIO_NUM,
        .pin_d7       = Y9_GPIO_NUM,
        .pin_d6       = Y8_GPIO_NUM,
        .pin_d5       = Y7_GPIO_NUM,
        .pin_d4       = Y6_GPIO_NUM,
        .pin_d3       = Y5_GPIO_NUM,
        .pin_d2       = Y4_GPIO_NUM,
        .pin_d1       = Y3_GPIO_NUM,
        .pin_d0       = Y2_GPIO_NUM,
        .pin_vsync    = VSYNC_GPIO_NUM,
        .pin_href     = HREF_GPIO_NUM,
        .pin_pclk     = PCLK_GPIO_NUM,

        .xclk_freq_hz = 20000000,           // 20MHz - stable on OV3660

        .ledc_timer   = LEDC_TIMER_2,
        .ledc_channel = LEDC_CHANNEL_2,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size   = FRAMESIZE_HQVGA,    // 240x176
        .jpeg_quality = 13,

        .fb_count    = 2,                   
        .fb_location = CAMERA_FB_IN_DRAM,   // DRAM not PSRAM - DMA accessible
        .grab_mode   = CAMERA_GRAB_LATEST
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s){
        s->set_framesize(s,     FRAMESIZE_QVGA);
        s->set_quality(s,       13);
        s->set_lenc(s,          0);              // Disable lens correction (slow)
        s->set_whitebal(s,      0);              // No AWB needed for IR
        s->set_exposure_ctrl(s, 0);              // Fixed exposure for IR lighting
        s->set_aec_value(s,     400);            // Tune for your IR LED strength (0-1200)
        s->set_gain_ctrl(s,     1);              // Auto gain on (do we want this off for better speed/less computation?)
        s->set_gainceiling(s,   GAINCEILING_8X); // High gain for night vision
    }

    ESP_LOGI(TAG, "Camera ready (OV3660 IR FPV, DRAM, ~33fps)");

    psram_ready = psram_alloc_frames();
     if (!psram_ready) {
        ESP_LOGE(TAG, "PSRAM alloc failed");
        return err;
    }

    return ESP_OK;
}

bool psram_alloc_frames(void) {
    // allocate space in pseudo static RAM for heavy work
    for (int i = 0; i < MAX_FRAMES; i++) {
        // Allocate to PSRAM
        frames[i].data = heap_caps_malloc(FRAME_BUF_SIZE, MALLOC_CAP_SPIRAM);
        frames[i].len  = 0;
        if (!frames[i].data) {
            ESP_LOGE(TAG, "PSRAM alloc failed at slot %d", i);
            // free all allocated frames and exit
            for (int j = 0; j < i; j++) {
                heap_caps_free(frames[j].data);
                frames[j].data = NULL;
            }
            return false;
        }
    }
    ESP_LOGI(TAG, "PSRAM ready — %d slots x %d bytes = %.1f MB",
             MAX_FRAMES, FRAME_BUF_SIZE,
             (float)MAX_FRAMES * FRAME_BUF_SIZE / (1024.0f * 1024.0f));
    return true;
}

void record_task(void *pv) {

    if (!psram_ready) {
        ESP_LOGE(TAG, "PSRAM not ready - aborting");
        vTaskDelete(NULL); // delete this task since PSRAM not ready
    }

    while(1) {
        int captured = 0;
        TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(RECORD_DURATION_MS);

        while (xTaskGetTickCount() < end && captured < MAX_FRAMES) {
            TickType_t frame_start = xTaskGetTickCount();

            camera_fb_t *frame_buffer = esp_camera_fb_get();
            if (!frame_buffer) {
                ESP_LOGW(TAG, "fb_get failed - skipping frame %d", captured);
            }

            size_t copy_len = frame_buffer->len < FRAME_BUF_SIZE ? frame_buffer->len : FRAME_BUF_SIZE;
            // copy frame buffer data to frames array
            memcpy(frames[captured].data, frame_buffer->buf, copy_len);
            frames[captured].len = copy_len;
            captured++;

            esp_camera_fb_return(frame_buffer);

            if (captured % 10 == 0) {
                ESP_LOGI(TAG, "  %d frames captured...", captured);
            }

            TickType_t elapsed = xTaskGetTickCount() - frame_start;
            TickType_t delay = pdMS_TO_TICKS(FRAME_INTERVAL_MS);
            vTaskDelay(elapsed < delay ? delay - elapsed : pdMS_TO_TICKS(5));
        }

        if (captured == 0) {
            ESP_LOGW(TAG, "No frames captured - skipping");
            continue;
        }
        
    }

}
