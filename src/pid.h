typedef struct {
    float kp, ki, kd;
    float integral;
    float prev_error;
    float output_min, output_max;
} pid_t;

void  pid_init(pid_t *pid, float kp, float ki, float kd,
               float out_min, float out_max);
float pid_update(pid_t *pid, float error, float dt);