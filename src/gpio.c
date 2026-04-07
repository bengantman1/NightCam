#include "gpio.h"

void IRAM_ATTR pir_isr(void* arg) {
    
}

void IRAM_ATTR btn_isr(void* arg) {
    
}


void gpio_init_all() {

    // PIR Pin init
    gpio_config_t pir = {
        .pin_bit_mask = (1ULL << PIR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&pir);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIR_PIN, pir_isr, NULL);

    // Button init
    gpio_config_t btn = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&btn);
    gpio_isr_handler_add(BUTTON_PIN, btn_isr, NULL);

    // IR array PWM init
    
    // config led timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_10_BIT,  // 0–1023
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 5000,               // 5 kHz PWM
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // config channel
    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL_0,
        .gpio_num = IR_ARRAY_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 512, // 50% duty cycle for 10 bit pwm timer
        .hpoint = 0, // phase = 0
    };
    ledc_channel_config(&ledc_channel);


}