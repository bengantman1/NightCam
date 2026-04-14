#include "esp_camera.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

// ========= CAMERA PINS (XIAO ESP32S3 Sense) =========
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// recording config
#define RECORD_DURATION_MS  5000
#define FRAME_INTERVAL_MS   30
#define MOUNT_POINT         "/sdcard"

#define MAX_FRAMES      150
#define FRAME_BUF_SIZE  25000 // worst case size of one frame in bytes

// function declarations
esp_err_t camera_init(void);
void record_task(void *pv);
bool psram_alloc_frames();