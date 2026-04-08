#include "gpio.h"

static const char* TAG = "MAIN MODULE"; // Tag for print statements

void app_main(void) {

    // Initialize Peripherals
    //camera_init();
    //sd_init();
    gpio_init_all(); // PIR, light-dependent-resistor, IR array, wifi button, servos

    while(1) {
        ESP_LOGI(TAG, "%d", ldr_read());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}