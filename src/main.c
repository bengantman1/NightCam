#include "gpio.h"
void app_main(void) {

    // Initialize Peripherals
    //camera_init();
    //sd_init();
    gpio_init_all(); // PIR, light-dependent-resistor, IR array, wifi button, servos


}