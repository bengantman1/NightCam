#pragma once // prevent multiple inclusions

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"  // MUST come first
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "global_events.h"
#include "esp_sleep.h"

// define pins
#define PIR_PIN      GPIO_NUM_4
#define BUTTON_PIN   GPIO_NUM_5
#define IR_ARRAY_PIN GPIO_NUM_6
#define LDR_ADC_CH   ADC_CHANNEL_1 // pin 1 for LDR

// define constants
#define DEBOUNCE_DELAY_US 200000  // 200 ms

extern adc_oneshot_unit_handle_t adc_handle;

/**
 * @brief Initialize ISRs, IR array digital out, and ADC for light-dependent-resistor reading
 */
void gpio_init_all(void);
/**
 * @brief Activate ISR when motion detected and notify recording task to begin capture using event group
 */
void pir_isr(void* arg); 
/**
 * @brief Activate on button press and notify wifi task to start access point
 */
void btn_isr(void* arg);