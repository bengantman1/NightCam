#include "gpio.h"

static const char* TAG = "MAIN MODULE"; // Tag for print statements

void app_main(void) {

    // Initialize Peripherals
    // Delay 5 seconds to let PIR stabilize
    vTaskDelay(pdMS_TO_TICKS(5000));

    //camera_init();
    //sd_init();
    gpio_init_all(); // PIR, light-dependent-resistor, IR array, wifi button, servos

    while(1) {
        int pin;
        // queue receives pin number from ISR that activated
        // blocks otherwise
        if (xQueueReceive(gpio_evt_queue, &pin, portMAX_DELAY)) {
            if (pin == PIR_PIN) ESP_LOGI(TAG, "PIR ISR");
            else if (pin == BUTTON_PIN) ESP_LOGI(TAG, "BUTTON ISR");
        }

        ESP_LOGI(TAG, "%d", ldr_read());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}