#include "pid.h"

void pid_init(PID_t *pid, float kp, float ki, float kd,
              float out_min, float out_max) {
    pid->kp = kp; pid->ki = ki; pid->kd = kd;
    pid->integral   = 0;
    pid->prev_error = 0;
    pid->output_min = out_min;
    pid->output_max = out_max;
}

float pid_update(PID_t *pid, float error, float dt) {
    pid->integral  += error * dt;
    float derivative = (error - pid->prev_error) / dt;
    pid->prev_error  = error;

    float out = pid->kp * error
              + pid->ki * pid->integral
              + pid->kd * derivative;

    // clamp
    if (out > pid->output_max) out = pid->output_max;
    if (out < pid->output_min) out = pid->output_min;
    return out;
}