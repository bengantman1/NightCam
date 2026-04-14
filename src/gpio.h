#pragma once // prevent multiple inclusions

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"  // MUST come first
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "global_events.h"

// define pins
#define PIR_PIN GPIO_NUM_4
#define BUTTON_PIN GPIO_NUM_5
#define IR_ARRAY_PIN GPIO_NUM_6
#define LDR_ADC_CH ADC_CHANNEL_1 // pin 1 for LDR

// define constants
#define DEBOUNCE_DELAY_US 200000  // 200 ms

// function prototypes
void gpio_init_all(void);
void pir_isr(void* arg); 
void btn_isr(void* arg);

/**
 * Values range from 1300-4095
 *  <= 2400 --> bright
 * > 2700 --> dark7
 */
void ldr_read_task(void *pv);