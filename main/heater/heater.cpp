#include "heater.hpp"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sensors/sensor_task.hpp"

static const char *TAG = "HEATER";

#define HEATER_PWM_GPIO         22
#define HEATER_PWM_FREQ_HZ      1000
#define HEATER_DUTY_RES         LEDC_TIMER_10_BIT
#define HEATER_MAX_DUTY         1023
#define HEATER_OVERTEMP_CUTOFF  60.0f

static float s_power = 0.0f;

static struct {
    float Kp;
    float Ki;
    float Kd;
    float integral;
    float prev_error;
} s_pid = {
    .Kp = 80.0f,
    .Ki = 1.0f,
    .Kd = 5.0f,
    .integral = 0.0f,
    .prev_error = 0.0f,
};

static void pid_reset(void)
{
    s_pid.integral = 0.0f;
    s_pid.prev_error = 0.0f;
}

static float pid_update(float setpoint, float current)
{
    float error = setpoint - current;

    float p_term = s_pid.Kp * error;
    float i_term = s_pid.Ki * s_pid.integral;
    float d_term = s_pid.Kd * (error - s_pid.prev_error);
    float output = p_term + i_term + d_term;

    if (output > HEATER_MAX_DUTY) {
        output = HEATER_MAX_DUTY;
    } else if (output < 0.0f) {
        output = 0.0f;
    } else {
        s_pid.integral += error;
    }

    s_pid.prev_error = error;
    return output;
}

static void heater_task_fn(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "PID task started (Kp=%.0f Ki=%.1f Kd=%.1f)",
             s_pid.Kp, s_pid.Ki, s_pid.Kd);

    while (true) {
        float current = sensor_get_current_temperature();
        float setpoint = sensor_get_target_temperature();
        uint32_t duty = 0;

        if (current >= HEATER_OVERTEMP_CUTOFF) {
            duty = 0;
            pid_reset();
            ESP_LOGW(TAG, "Overtemp cutoff: %.1fC >= %.0fC",
                     (double)current, (double)HEATER_OVERTEMP_CUTOFF);
        } else {
            float raw = pid_update(setpoint, current);
            duty = (uint32_t)(raw + 0.5f);
            if (duty > HEATER_MAX_DUTY) duty = HEATER_MAX_DUTY;
        }

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        s_power = (float)duty / (float)HEATER_MAX_DUTY;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t heater_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = HEATER_DUTY_RES,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = HEATER_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "timer config failed: %s", esp_err_to_name(err));
        return err;
    }

    ledc_channel_config_t ch = {
        .gpio_num = HEATER_PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = { .output_invert = 0 },
        .deconfigure = false,
    };
    err = ledc_channel_config(&ch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "channel config failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "PWM ready (GPIO%d, %dHz, 10-bit)", HEATER_PWM_GPIO, HEATER_PWM_FREQ_HZ);
    return ESP_OK;
}

esp_err_t heater_start_pid(void)
{
    BaseType_t ret = xTaskCreatePinnedToCore(
        heater_task_fn, "heater_pid", 4096, NULL, 5, NULL, 0);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "failed to create task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

float heater_get_power(void)
{
    return s_power;
}
