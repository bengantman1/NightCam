#include "camera.h"
#include "gpio.h"

static const char* TAG = "CAMERA"; // Tag for print statements

static frame_buf_t frames[MAX_FRAMES];
static bool        psram_ready = false;
static sdmmc_card_t *sd_card = NULL;

QueueHandle_t frame_queue;

esp_err_t sd_init(void) {
    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files              = 5,
        .allocation_unit_size   = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus = {
        .mosi_io_num     = SD_MOSI,
        .miso_io_num     = SD_MISO,
        .sclk_io_num     = SD_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096
    };

    esp_err_t ret = spi_bus_initialize(host.slot, &bus, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: 0x%x", ret);
        return ret;
    }

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = SD_CS;
    slot.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot, &mount_cfg, &sd_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: 0x%x", ret);
        spi_bus_free(host.slot);
        return ret;
    }

    sdmmc_card_print_info(stdout, sd_card);
    ESP_LOGI(TAG, "SD mounted OK");
    return ESP_OK;
}

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
        .frame_size   = FRAMESIZE_QVGA,    // 320 x 240
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
//vary some of these dependent on Light sensor reading
    sensor_t *s = esp_camera_sensor_get();
    if (s){
        s->set_framesize(s,     FRAMESIZE_QVGA);
        s->set_vflip(s,         1);
        s->set_hmirror(s,       1);
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

    frame_queue = xQueueCreate(10, sizeof(frame_buf_t));

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

static int find_next_clip_index(void) {
    int i = 0;
    struct stat st;
    char path[48];
    while (i < 9999) {
        snprintf(path, sizeof(path), MOUNT_POINT "/C%04d", i);
        // if the path doesn't exist, index i is available
        if (stat(path, &st) != 0) return i;
        i++;
    }
    return i;
}

void record_task(void *pv) {

    if (!psram_ready) {
        ESP_LOGE(TAG, "PSRAM not ready - aborting");
        vTaskDelete(NULL); // delete this task since PSRAM not ready
    }
    int clip_index = find_next_clip_index();
    while(1) {
        // Turn on IR Array if necessary
        int raw;
        adc_oneshot_read(adc_handle, LDR_ADC_CH, &raw);
        if (raw > 900) {
            gpio_set_level(IR_ARRAY_PIN, 1);
            ESP_LOGI(TAG, "Raw ADC Value: %d, IR array ON", raw);
        } else {
            gpio_set_level(IR_ARRAY_PIN, 0);
            ESP_LOGI(TAG, "Raw ADC Value: %d, IR array OFF", raw);
        }
        // set camera as active so PIR cannot interrupt
        xEventGroupSetBits(event_group, CAMERA_ACTIVE);
        
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
            // send frame pointers to tracking process for servo updates
            xQueueSend(frame_queue, &frames[captured], 0);
            captured++;

            esp_camera_fb_return(frame_buffer);

            if (captured % 10 == 0) {
                ESP_LOGI(TAG, "  %d frames captured...", captured);
            }

            TickType_t elapsed = xTaskGetTickCount() - frame_start;
            TickType_t delay = pdMS_TO_TICKS(FRAME_INTERVAL_MS);
            vTaskDelay(elapsed < delay ? delay - elapsed : pdMS_TO_TICKS(5));
        }

        // Turn off IR array now that camera is done recording
        gpio_set_level(IR_ARRAY_PIN, 0);

        if (captured == 0) {
            ESP_LOGW(TAG, "No frames captured - skipping");
            continue;
        }
        
        ESP_LOGI(TAG, "Captured %d frames — writing to SD", captured);

        // create directory for clips
        char dir[48];
        // write mount point to directory buffer. There should be a new folder each wakeup
        snprintf(dir, sizeof(dir), MOUNT_POINT"/C%04d", clip_index);

        if (mkdir(dir, 0775) != 0) {
            ESP_LOGE(TAG, "mkdir failed: %s - errno: %d (%s)", dir, errno, strerror(errno));
        } else {
            ESP_LOGI(TAG, "Created directory: %s", dir);
        }
        
        // Write from PSRAM to SD card
        int saved = 0;
        for (int i = 0; i < captured; i++) {
            char path[72];
            snprintf(path, sizeof(path), "%s/%05d.jpg", dir, i);

            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(frames[i].data, 1, frames[i].len, f);
                fclose(f);
                saved++;
            } else {
                ESP_LOGE(TAG, "fopen failed: %s", path);
            }

            if ((i + 1) % 10 == 0) {
                ESP_LOGI(TAG, "  written %d/%d frames", i + 1, captured);
            }
        }

        ESP_LOGI(TAG, ">>> Clip %d done - %d/%d frames saved to %s",
                 clip_index, saved, captured, dir);

        clip_index++; // increment clip index by 1 afterwards

        
        // Mark camera task as inactive
        xEventGroupClearBits(event_group, CAMERA_ACTIVE);
        xEventGroupSetBits(event_group, RECORDING_DONE);
    }

}
