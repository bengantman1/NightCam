#include "gpio.h"
#include "camera.h"
#include "esp_sleep.h"
#include "servo.h"

static const char* TAG = "MAIN"; // Tag for print statements

typedef enum { LIGHT_DAY, LIGHT_DUSK, LIGHT_NIGHT } light_level_t;

void app_main(void) {

    // Initialize Peripherals
    // Delay 5 seconds to let PIR stabilize
    vTaskDelay(pdMS_TO_TICKS(5000));

    const char* wakeup_reason;
    uint32_t wakeup_causes = esp_sleep_get_wakeup_causes();
    // based on wake up cause (button, PIR, first boot), turn on WIFI mode or capture mode


    camera_init();
    sd_init();
    //servo_init();
    gpio_init_all(); // PIR, light-dependent-resistor, IR array, wifi button, servos

    xTaskCreatePinnedToCore(record_task, "Record_Task", 4096, NULL, 2, NULL, 1); // higher number is higher priority
    xTaskCreatePinnedToCore(ldr_read_task, "LDR_Read", 4096, NULL, 1, NULL, 1);
    /**
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
    }*/
}