#include "servo.h"

static uint32_t us_to_duty(uint32_t us) {
    // period = 1,000,000 / 50Hz = 20,000 us
    return (uint32_t)((uint64_t)LEDC_MAX_DUTY * us / 20000);
}

static void ledc_setup(ledc_timer_t timer, ledc_channel_t channel, int gpio) {
    // Servo Init
    ledc_timer_config_t tc = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = timer,
        .duty_resolution = LEDC_RESOLUTION,
        .freq_hz         = SERVO_PWM_FREQ,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ledc_timer_config(&tc);

    ledc_channel_config_t cc = {
        .gpio_num   = gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = channel,
        .timer_sel  = timer,
        .duty       = us_to_duty(SERVO_MID_US),
        .hpoint     = 0
    };
    ledc_channel_config(&cc);
}

void servo_init(void) {
    ledc_setup(PAN_TIMER,  PAN_CHANNEL,  SERVO_PAN_PIN);
    ledc_setup(TILT_TIMER, TILT_CHANNEL, SERVO_TILT_PIN);
}

static void servo_write_us(ledc_channel_t ch, uint32_t us) {
    uint32_t duty = us_to_duty(us);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

static uint32_t deg_to_us(float deg) {
    // Map -90..+90 → 1000..2000 µs
    float clamped = deg < -90.0f ? -90.0f : (deg > 90.0f ? 90.0f : deg);
    return (uint32_t)(SERVO_MID_US + (clamped / 90.0f) * ((SERVO_MAX_US - SERVO_MIN_US) / 2.0f));
}

void servo_set_pan(float deg)  { servo_write_us(PAN_CHANNEL,  deg_to_us(deg)); }
void servo_set_tilt(float deg) { servo_write_us(TILT_CHANNEL, deg_to_us(deg)); }