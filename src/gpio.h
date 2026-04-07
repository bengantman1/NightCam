#pragma once // prevent multiple inclusions

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"  // MUST come first
#include "freertos/task.h"
//#include "esp_adc/adc_oneshot.h"

#define PIR_PIN GPIO_NUM_4
#define BUTTON_PIN GPIO_NUM_5
#define IR_ARRAY_PIN GPIO_NUM_6
//#define LDR_ADC_CH ADC_CHANNEL_3 //TODO: fix

void gpio_init_all(void);
void pir_isr(void* arg); 
void btn_isr(void* arg);