#pragma once // prevent multiple inclusions

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"  // MUST come first
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

// define pins
#define PIR_PIN GPIO_NUM_4
#define BUTTON_PIN GPIO_NUM_5
#define IR_ARRAY_PIN GPIO_NUM_6
#define LDR_ADC_CH ADC_CHANNEL_0 // pin 0 for LDR

// define constants
#define DEBOUNCE_DELAY_US 200000  // 200 ms

// define handles
extern QueueHandle_t gpio_evt_queue;

// function prototypes
void gpio_init_all(void);
void pir_isr(void* arg); 
void btn_isr(void* arg);
int ldr_read(void);