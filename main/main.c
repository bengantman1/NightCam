#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

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

// ========= MOTOR BUTTONS =========
#define forward_b   GPIO_NUM_9
#define reverse_b   GPIO_NUM_1
#define left_b      GPIO_NUM_8
#define right_b     GPIO_NUM_7

// ========= MOTOR DRIVER =========
#define MOTOR_ENA   GPIO_NUM_5
#define IN1         GPIO_NUM_3
#define IN3         GPIO_NUM_4

// ========= SERVO =========
#define SERVO_PWM   GPIO_NUM_2

// ========= PWM =========
#define MOTOR_PWM_FREQ 1000
#define SERVO_PWM_FREQ 50
#define PWM_RES LEDC_TIMER_13_BIT
#define PWM_OFF 0

#define SERVO_LEFT_DUTY   1003
#define SERVO_CENTER_DUTY 614
#define SERVO_RIGHT_DUTY  225

#define LEDC_MODE   LEDC_LOW_SPEED_MODE
#define TIMER_MOTOR LEDC_TIMER_0
#define TIMER_SERVO LEDC_TIMER_1
#define CH_MOTOR    LEDC_CHANNEL_0
#define CH_SERVO    LEDC_CHANNEL_1

// ========= WIFI =========
#define AP_SSID "sweet_jp"
#define AP_PASS "redline_54321"

// ========= STREAM =========
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY     = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART         = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ~30ms between captures = ~33fps, matches OV3660 HQVGA output rate at 20MHz XCLK
// Prevents cam_hal timeout warnings caused by hammering fb_get faster than sensor delivers
#define FRAME_INTERVAL_MS 30

static const char *TAG = "cam_motor";

// Depth 1 — always holds the newest frame only
static QueueHandle_t frame_queue = NULL;

static volatile bool web_forward = false;
static volatile bool web_reverse = false;
static volatile bool web_left    = false;
static volatile bool web_right   = false;

// ================= PWM =================
static void set_pwm(int ch, uint32_t duty){
    ledc_set_duty(LEDC_MODE, ch, duty);
    ledc_update_duty(LEDC_MODE, ch);
}

// ================= MOTOR =================
void motor_setup(){
    gpio_config_t btn = {
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pin_bit_mask = (1ULL<<forward_b)|(1ULL<<reverse_b)|(1ULL<<left_b)|(1ULL<<right_b)
    };
    gpio_config(&btn);

    gpio_config_t dirs = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL<<IN1)|(1ULL<<IN3)
    };
    gpio_config(&dirs);

    ledc_timer_config_t mt = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = TIMER_MOTOR,
        .freq_hz         = MOTOR_PWM_FREQ,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ledc_timer_config(&mt);

    ledc_timer_config_t st = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = TIMER_SERVO,
        .freq_hz         = SERVO_PWM_FREQ,
        .duty_resolution = PWM_RES,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ledc_timer_config(&st);

    ledc_channel_config_t mc = {
        .channel    = CH_MOTOR,
        .gpio_num   = MOTOR_ENA,
        .speed_mode = LEDC_MODE,
        .timer_sel  = TIMER_MOTOR,
        .duty       = 0
    };
    ledc_channel_config(&mc);

    ledc_channel_config_t sc = {
        .channel    = CH_SERVO,
        .gpio_num   = SERVO_PWM,
        .speed_mode = LEDC_MODE,
        .timer_sel  = TIMER_SERVO,
        .duty       = SERVO_CENTER_DUTY
    };
    ledc_channel_config(&sc);
}

void control_logic(){
    bool pf  = (gpio_get_level(forward_b) == 0) || web_forward;
    bool pr  = (gpio_get_level(reverse_b) == 0) || web_reverse;
    bool pl  = (gpio_get_level(left_b)    == 0) || web_left;
    bool prt = (gpio_get_level(right_b)   == 0) || web_right;

    if (pf) {
        gpio_set_level(IN1, 1);
        gpio_set_level(IN3, 0);
        set_pwm(CH_MOTOR, 1023);
    } else if (pr) {
        gpio_set_level(IN1, 0);
        gpio_set_level(IN3, 1);
        set_pwm(CH_MOTOR, 1023);
    } else {
        gpio_set_level(IN1, 0);
        gpio_set_level(IN3, 0);
        set_pwm(CH_MOTOR, PWM_OFF);
    }

    if (pl)       set_pwm(CH_SERVO, SERVO_LEFT_DUTY);
    else if (prt) set_pwm(CH_SERVO, SERVO_RIGHT_DUTY);
    else          set_pwm(CH_SERVO, SERVO_CENTER_DUTY);
}

void motor_task(void *pv){
    while(1){
        control_logic();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ================= CAMERA CAPTURE =================
void camera_capture_task(void *pv){
    ESP_LOGI(TAG, "Camera capture task core %d", xPortGetCoreID());
    while(1){
        camera_fb_t *fb = esp_camera_fb_get();

        if (!fb){
            // Short backoff on failure — avoids log spam and lets sensor recover
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        camera_fb_t *old = NULL;
        // Drop stale frame if stream handler hasn't consumed it yet
        if (xQueueReceive(frame_queue, &old, 0) == pdTRUE)
            esp_camera_fb_return(old);

        xQueueSend(frame_queue, &fb, 0);

        // Pace to ~33fps — prevents hammering fb_get faster than OV3660 produces frames
        // which causes "Failed to get frame: timeout" warnings in cam_hal
        vTaskDelay(pdMS_TO_TICKS(FRAME_INTERVAL_MS));
    }
}

// ================= STREAM =================
esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t *fb = NULL;
    esp_err_t res   = ESP_OK;
    char part_buf[64];

    httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    while(true){
        if (xQueueReceive(frame_queue, &fb, pdMS_TO_TICKS(1000)) != pdTRUE)
            break;

        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));

        if (res == ESP_OK){
            size_t hlen = snprintf(part_buf, 64, _STREAM_PART, fb->len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }

        if (res == ESP_OK)
            res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);

        esp_camera_fb_return(fb);
        if (res != ESP_OK) break;
    }

    return res;
}

// ================= HTTP =================
static const char HTML[] =
    "<html><body style='margin:0;background:black'>"
    "<img src='/stream' style='width:100%;height:100vh;object-fit:contain'>"
    "</body></html>";

esp_err_t root_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML, strlen(HTML));
    return ESP_OK;
}

void start_server(){
    httpd_config_t config    = HTTPD_DEFAULT_CONFIG();
    config.core_id           = 1;   // Server on Core 1, away from motor task on Core 0
    config.recv_wait_timeout = 1;
    config.send_wait_timeout = 1;
    config.max_uri_handlers  = 8;

    httpd_handle_t server = NULL;

    httpd_uri_t root   = { "/",       HTTP_GET, root_handler,   NULL };
    httpd_uri_t stream = { "/stream", HTTP_GET, stream_handler, NULL };

    if (httpd_start(&server, &config) == ESP_OK){
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &stream);
        ESP_LOGI(TAG, "HTTP server started on Core 1");
    } else {
        ESP_LOGE(TAG, "HTTP server failed to start");
    }
}

// ================= WIFI =================
void wifi_init_ap(){
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_AP);

    wifi_config_t ap = {
        .ap = {
            .ssid           = AP_SSID,
            .ssid_len       = strlen(AP_SSID),
            .password       = AP_PASS,
            .channel        = 6,
            .max_connection = 4,
            .authmode       = WIFI_AUTH_WPA2_PSK
        }
    };

    esp_wifi_set_config(WIFI_IF_AP, &ap);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE);  // Disable power saving — eliminates 100ms+ latency spikes

    ESP_LOGI(TAG, "WiFi AP started: %s", AP_SSID);
}

// ================= CAMERA INIT =================
void camera_init(){
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

        .xclk_freq_hz = 20000000,           // 20MHz — stable on OV3660

        .ledc_timer   = LEDC_TIMER_2,
        .ledc_channel = LEDC_CHANNEL_2,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size   = FRAMESIZE_HQVGA,    // 240x176
        .jpeg_quality = 15,

        .fb_count    = 1,                   // Single buffer — camera DMA requires DRAM
        .fb_location = CAMERA_FB_IN_DRAM,   // DRAM not PSRAM — DMA accessible
        .grab_mode   = CAMERA_GRAB_LATEST
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s){
        s->set_framesize(s,     FRAMESIZE_QVGA);
        s->set_quality(s,       13);
        s->set_lenc(s,          0);              // Disable lens correction (slow)
        s->set_whitebal(s,      0);              // No AWB needed for IR
        s->set_exposure_ctrl(s, 0);              // Fixed exposure for IR lighting
        s->set_aec_value(s,     400);            // Tune for your IR LED strength (0-1200)
        s->set_gain_ctrl(s,     1);              // Auto gain on
        s->set_gainceiling(s,   GAINCEILING_8X); // High gain for night vision
    }

    ESP_LOGI(TAG, "Camera ready (OV3660 IR FPV, DRAM, ~33fps)");
}

// ================= MAIN =================
void app_main(){
    nvs_flash_init();

    // Camera FIRST — allocates DMA frame buffer before WiFi stack consumes DRAM
    camera_init();
    wifi_init_ap();
    motor_setup();

    frame_queue = xQueueCreate(1, sizeof(camera_fb_t*));
    if (!frame_queue){
        ESP_LOGE(TAG, "Failed to create frame queue");
        return;
    }

    // Core 0: motor control (priority 5)
    xTaskCreatePinnedToCore(motor_task,          "motor", 4096, NULL, 5, NULL, 0);

    // Core 1: camera capture (priority 6) + HTTP server (config.core_id = 1)
    xTaskCreatePinnedToCore(camera_capture_task, "cam",   4096, NULL, 6, NULL, 1);

    start_server();

    ESP_LOGI(TAG, "System ready -> http://192.168.4.1");
}