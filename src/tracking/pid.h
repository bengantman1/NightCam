// Holds PID state for one motor
typedef struct {
    float kp, ki, kd;
    float integral;
    float prev_error;
    float output_min, output_max;
} PID_t;

/**
 * @brief Initialize PID state based on PID parameters and set integral, error values to 0
 */
void pid_init(PID_t *pid, float kp, float ki, float kd, float out_min, float out_max);

/**
 * @brief Update motor positions based on error to improve motor stability, smoothness, and accuracy.
 */
float pid_update(PID_t *pid, float error, float dt);