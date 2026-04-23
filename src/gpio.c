#include "gpio.h"

// globals
static const char* TAG = "GPIO"; // Tag for print statements

static int64_t last_pir_isr_time = 0;
adc_oneshot_unit_handle_t adc_handle; 
EventGroupHandle_t event_group;

void IRAM_ATTR pir_isr(void* arg) {
    EventBits_t state = xEventGroupGetBitsFromISR(event_group);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // only run if no other important tasks are running
    if (!(state & (PIR_ACTIVATED | CAMERA_ACTIVE | WIFI_ACTIVE))) {
        int64_t now = esp_timer_get_time();
        if (now - last_pir_isr_time > DEBOUNCE_DELAY_US) {
            last_pir_isr_time = now;
            // Notify task that does the work
            xEventGroupSetBitsFromISR(event_group, PIR_ACTIVATED, &xHigherPriorityTaskWoken);
        }
    }
}

static int64_t last_btn_isr_time = 0;

void IRAM_ATTR btn_isr(void* arg) {
    EventBits_t state = xEventGroupGetBitsFromISR(event_group);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (!(state & (PIR_ACTIVATED | CAMERA_ACTIVE | WIFI_ACTIVE))) {
        int64_t now = esp_timer_get_time();
        if (now - last_btn_isr_time > DEBOUNCE_DELAY_US) {
            last_btn_isr_time = now;

            xEventGroupSetBitsFromISR(event_group, WIFI_ACTIVE, &xHigherPriorityTaskWoken);
        }
    }
}

void gpio_init_all() {

    event_group = xEventGroupCreate(); // 24 event bits

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

    // IR array init
    gpio_config_t irarray = {
        .pin_bit_mask = (1ULL << IR_ARRAY_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&irarray);
    gpio_set_level(IR_ARRAY_PIN, 0);

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