#include "gpio.h"

// globals
static const char* TAG = "GPIO"; // Tag for print statements
static adc_oneshot_unit_handle_t adc_handle;  // 'static' limits scope to this file
static int64_t last_pir_isr_time = 0;
QueueHandle_t gpio_evt_queue;

void IRAM_ATTR pir_isr(void* arg) {
    int64_t now = esp_timer_get_time();
    if (now - last_pir_isr_time > DEBOUNCE_DELAY_US) {
        last_pir_isr_time = now;
        int pin = PIR_PIN;
        // Notify task that does the work
        xQueueSendFromISR(gpio_evt_queue, &pin, NULL);
    }
}

static int64_t last_btn_isr_time = 0;

void IRAM_ATTR btn_isr(void* arg) {
    int64_t now = esp_timer_get_time();
    if (now - last_btn_isr_time > DEBOUNCE_DELAY_US) {
        last_btn_isr_time = now;
        int pin = BUTTON_PIN;
        xQueueSendFromISR(gpio_evt_queue, &pin, NULL);
    }
}

void gpio_init_all() {
    // init queue for ISR communication
    gpio_evt_queue = xQueueCreate(10, sizeof(int));

    // PIR Pin init
    gpio_config_t pir = {
        .pin_bit_mask = (1ULL << PIR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&pir);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIR_PIN, pir_isr, NULL);

    // Button init
    gpio_config_t btn = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
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
        .clk_cfg          = LEDC_AUTO_CLK,
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

    // config light dependent resistor ADC
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12, // attenuation to support 0 - 3.1V
        .bitwidth = ADC_BITWIDTH_12 // 12 bit output
    };
    adc_oneshot_config_channel(adc_handle, LDR_ADC_CH, &chan_cfg); // set pin to LDR_ADC_CH
}

int ldr_read(void) {
    int raw; 
    adc_oneshot_read(adc_handle, LDR_ADC_CH, &raw);
    return raw;
}

// define servo task