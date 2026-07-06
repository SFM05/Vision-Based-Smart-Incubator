#include "tcd.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensor.h"
#include <math.h>
#include <stdio.h>

// ================= 系统参数配置 =================
constexpr uint32_t PWM_FREQ_HZ = 5000;
constexpr uint16_t PWM_DUTY_RES = 13; // 0 ~ 8191

constexpr float TARGET_TEMP = 30.0f;
constexpr float MAX_SAFE_TEMP = 45.0f;
constexpr uint32_t CONTROL_INTERVAL_MS = 1000;

constexpr float MAX_HEATER_POWER = 95.0f;
constexpr float MIN_HEAT_POWER = 80.0f;
constexpr float PID_SWITCH_THRESHOLD = 0.5f;

constexpr float PID_KP = 10.0f;
constexpr float PID_KI = 0.02f;
constexpr float PID_KD = 5.0f;
constexpr float PID_INTEGRAL_MAX = 40.0f;
constexpr float OUTPUT_RATE_LIMIT = 10.0f;

static const char *TAG = "TCD";

// ================= 数据结构 =================
typedef struct
{
    float kp, ki, kd;
    float setpoint;
    float integral;
    float prev_error;
    float prev_measurement;
    float output;
    float out_min, out_max;
    float integral_max;
} PID_Controller;

static PID_Controller pid;

// ================= PID 算法 =================
static void PID_Init(PID_Controller *pid, float kp, float ki, float kd, float int_max, float max_power)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->integral_max = int_max;
    pid->out_min = 0.0f;
    pid->out_max = max_power;
    pid->integral = 0;
    pid->prev_error = 0;
    pid->prev_measurement = 0;
    pid->output = 0;
}

static float PID_Compute(PID_Controller *pid, float measurement)
{
    float error = pid->setpoint - measurement;
    float p_out = pid->kp * error;

    bool is_overshoot_braking = false;
    if (error < 0 && pid->output > 0)
        is_overshoot_braking = true;

    if (!is_overshoot_braking)
    {
        pid->integral += error;
        if (pid->integral > pid->integral_max)
            pid->integral = pid->integral_max;
        if (pid->integral < -pid->integral_max)
            pid->integral = -pid->integral_max;
    }

    float i_out = pid->ki * pid->integral;
    float d_measurement = measurement - pid->prev_measurement;
    float d_out = -pid->kd * d_measurement;

    float raw_output = p_out + i_out + d_out;

    float delta = raw_output - pid->output;
    if (delta > OUTPUT_RATE_LIMIT)
        delta = OUTPUT_RATE_LIMIT;
    if (delta < -OUTPUT_RATE_LIMIT)
        delta = -OUTPUT_RATE_LIMIT;

    pid->output += delta;

    if (pid->output > pid->out_max)
        pid->output = pid->out_max;
    if (pid->output < pid->out_min)
        pid->output = pid->out_min;

    pid->prev_error = error;
    pid->prev_measurement = measurement;

    return pid->output;
}

// ================= 辅助函数 =================
static void pwm_set_percent(float percent)
{
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;
    uint32_t max_duty = (1 << PWM_DUTY_RES) - 1;
    uint32_t duty = (uint32_t)(percent * max_duty / 100.0f);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// ================= 硬件初始化 =================
static void tcd_hw_init(gpio_num_t heater)
{
    // 1. 初始化 PWM (LED Controller) - 使用逐个赋值避免 C++ 编译顺序错误
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_timer.timer_num = LEDC_TIMER_0;
    ledc_timer.duty_resolution = (ledc_timer_bit_t)PWM_DUTY_RES;
    ledc_timer.freq_hz = PWM_FREQ_HZ;
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer.deconfigure = false;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {};
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel.channel = LEDC_CHANNEL_0;
    ledc_channel.gpio_num = heater;
    ledc_channel.timer_sel = LEDC_TIMER_0;
    ledc_channel.intr_type = LEDC_INTR_DISABLE;
    ledc_channel.duty = 0;
    ledc_channel.hpoint = 0;
    ledc_channel.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // 3. 初始化 PID
    PID_Init(&pid, PID_KP, PID_KI, PID_KD, PID_INTEGRAL_MAX, MAX_HEATER_POWER);
    pid.setpoint = TARGET_TEMP;

    ESP_LOGI(TAG, "Init OK (PWM@GPIO%d)", heater);
}

// ================= 后台任务 =================
static void tcd_loop_task(void *pvParameters)
{
    float temp = 0.0f;
    bool last_was_fast_mode = true;

    while (1)
    {
        sensors_data_t data;
        sensors_driver_read(&data);

        if (data.sht_valid)
        {
            temp = data.temperature_c;
            float error = pid.setpoint - temp;
            float final_power = 0;
            bool is_fast_mode = (fabs(error) > PID_SWITCH_THRESHOLD);

            if (temp > MAX_SAFE_TEMP)
            {
                final_power = 0;
                pid.integral = 0;
                ESP_LOGE(TAG, "!!! Over Temp (%.1f°C) !!!", temp);
            }
            else if (is_fast_mode)
            {
                final_power = (error > 0) ? MAX_HEATER_POWER : 0.0f;
                if (!last_was_fast_mode)
                {
                    pid.integral = 0;
                    pid.prev_error = error;
                }
            }
            else
            {
                if (last_was_fast_mode)
                {
                    float target_initial_power = 80.0f;
                    float initial_integral = target_initial_power / pid.ki;
                    if (initial_integral > pid.integral_max)
                        initial_integral = pid.integral_max;

                    pid.integral = initial_integral;
                    pid.output = MAX_HEATER_POWER;
                    pid.prev_measurement = temp;
                }

                final_power = PID_Compute(&pid, temp);

                if (error > 0 && final_power < MIN_HEAT_POWER)
                {
                    final_power = MIN_HEAT_POWER;
                    pid.output = MIN_HEAT_POWER;
                }
            }

            last_was_fast_mode = is_fast_mode;
            pwm_set_percent(final_power);

            ESP_LOGI(TAG, "T:%.2f | %s | Out:%.1f%%",
                     temp, is_fast_mode ? "FAST" : "PID", final_power);
        }
        else
        {
            ESP_LOGW(TAG, "Sensor Read Failed");
            pwm_set_percent(0);
        }

        vTaskDelay(pdMS_TO_TICKS(CONTROL_INTERVAL_MS));
    }
}

// ================= 对外接口 =================
void tcd_set_target(float target_c)
{
    pid.setpoint = target_c;
    ESP_LOGI(TAG, "Target set to %.1f°C", target_c);
}

void tcd_start(gpio_num_t heater_pin)
{
    ESP_LOGI(TAG, "Starting TCD System...");
    tcd_hw_init(heater_pin);
    xTaskCreate(tcd_loop_task,
                "tcd_loop",
                4096,
                NULL,
                5,
                NULL);
}