#include "driver/ledc.h"
#include "esp_timer.h"

#define SERVO_PWM_FREQ   50
#define SERVO_MIN_US     500  // full left / full down
#define SERVO_MID_US     1500  // center
#define SERVO_MAX_US     2500  // full right / full up

#define LEDC_RESOLUTION  LEDC_TIMER_14_BIT 
#define LEDC_RESOLUTION_BITS 14
#define LEDC_MAX_DUTY ((1 << LEDC_RESOLUTION_BITS) - 1) // duty cycle of 100%

#define SERVO_PAN_PIN GPIO_NUM_1
#define SERVO_TILT_PIN GPIO_NUM_44

#define PAN_TIMER    LEDC_TIMER_0
#define PAN_CHANNEL  LEDC_CHANNEL_0
#define TILT_TIMER   LEDC_TIMER_1
#define TILT_CHANNEL LEDC_CHANNEL_1

/**
 * @brief Configure PWM channels and pins for servo control
 */
void servo_init(void);

/**
 * @brief Set camera pan angle. -90 to 90 degrees.
 */
void servo_set_pan(float deg);

/**
 * @brief Set camera tilt angle. -90 to 90 degrees.
 */
void servo_set_tilt(float deg);