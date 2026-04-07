#pragma once // prevent multiple inclusions

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"  // MUST come first
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
//#include "esp_adc/adc_oneshot.h"

// define pins
#define PIR_PIN GPIO_NUM_4
#define BUTTON_PIN GPIO_NUM_5
#define IR_ARRAY_PIN GPIO_NUM_6
//#define LDR_ADC_CH ADC_CHANNEL_3 //TODO: fix

// define constants
#define DEBOUNCE_DELAY_US 20000  // 20 ms

void gpio_init_all(void);
void pir_isr(void* arg); 
void btn_isr(void* arg);