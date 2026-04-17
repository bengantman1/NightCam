#include "driver/ledc.h"
#include "esp_timer.h"

#define SERVO_PWM_FREQ   50
#define SERVO_MIN_US     1000  // full left / full down
#define SERVO_MID_US     1500  // center
#define SERVO_MAX_US     2000  // full right / full up
#define LEDC_MAX_DUTY    16383

#define SERVO_PAN_PIN GPIO_NUM_1
#define SERVO_TILT_PIN GPIO_NUM_44

#define PAN_TIMER    LEDC_TIMER_0
#define PAN_CHANNEL  LEDC_CHANNEL_0
#define TILT_TIMER   LEDC_TIMER_1
#define TILT_CHANNEL LEDC_CHANNEL_1

#define LEDC_RESOLUTION  LEDC_TIMER_14_BIT 

void servo_init(void);
void servo_set_pan(float deg);   // -90 to +90
void servo_set_tilt(float deg);