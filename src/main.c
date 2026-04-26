#include "gpio.h"
#include "camera.h"
#include "wifi_server.h"
#include "tracking/tracking.h"

void app_main(void) {

    // Initialize Peripherals
    // Delay 5 seconds to let PIR stabilize
    vTaskDelay(pdMS_TO_TICKS(5000));

    // init all peripherals
    camera_init();
    sd_init();
    gpio_init_all(); // PIR, light-dependent-resistor, IR array, wifi button, servos
    tracker_init();

    // Create tasks with tracker/wifi on faster core and wifi at highest priority
    xTaskCreatePinnedToCore(record_task,      "Record_Task", 4096, NULL, 1, NULL, 1); // higher number is higher priority
    xTaskCreatePinnedToCore(wifi_server_task, "Wifi_Server", 8192, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(tracker_task,     "Tracker",     4096, NULL, 1, NULL, 0); // pin to core 0 since it will not run while WIFI is on

}