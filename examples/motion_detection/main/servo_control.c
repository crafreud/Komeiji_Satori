/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "servo_control.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SERVO_CONTROL";

// 舵机GPIO引脚数组
static const int servo_gpio[SERVO_MAX] = {
    SERVO_1_PIN,
    SERVO_2_PIN, 
    SERVO_3_PIN,
    SERVO_4_PIN
};

// LEDC通道配置
static const ledc_channel_t servo_channel[SERVO_MAX] = {
    LEDC_CHANNEL_1,
    LEDC_CHANNEL_2,
    LEDC_CHANNEL_3,
    LEDC_CHANNEL_4
};

// 当前舵机角度记录
static uint32_t current_angles[SERVO_MAX] = {90, 90, 90, 90}; // 初始化为中位

// 平滑移动参数
#define SMOOTH_STEP 2  // 每次移动的最大角度步长
#define MOVE_DELAY_MS 20  // 移动间隔时间

/**
 * @brief 角度转换为PWM占空比
 */
static uint32_t angle_to_duty(uint32_t angle)
{
    if (angle > SERVO_MAX_DEGREE) {
        angle = SERVO_MAX_DEGREE;
    }
    
    // 计算脉宽 (微秒)
    uint32_t pulse_width_us = SERVO_MIN_PULSEWIDTH_US + 
        (angle * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US)) / SERVO_MAX_DEGREE;
    
    // 转换为占空比 (13位分辨率)
    uint32_t duty = (pulse_width_us * 8192) / (1000000 / SERVO_FREQ_HZ);
    
    return duty;
}

esp_err_t servo_control_init(void)
{
    esp_err_t ret = ESP_OK;
    
    // 配置LEDC定时器
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = SERVO_FREQ_HZ,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置每个舵机的LEDC通道
    for (int i = 0; i < SERVO_MAX; i++) {
        ledc_channel_config_t ledc_channel_cfg = {
            .channel = servo_channel[i],
            .duty = angle_to_duty(90), // 初始化为90度中位
            .gpio_num = servo_gpio[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .hpoint = 0,
            .timer_sel = LEDC_TIMER_1,
        };
        ret = ledc_channel_config(&ledc_channel_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LEDC channel %d config failed: %s", i, esp_err_to_name(ret));
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "Servo control initialized successfully");
    return ESP_OK;
}

esp_err_t servo_set_angle(servo_id_t servo_id, uint32_t angle)
{
    if (servo_id >= SERVO_MAX) {
        ESP_LOGE(TAG, "Invalid servo ID: %d", servo_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (angle > SERVO_MAX_DEGREE) {
        ESP_LOGW(TAG, "Angle %lu exceeds maximum, clamping to %d", angle, SERVO_MAX_DEGREE);
        angle = SERVO_MAX_DEGREE;
    }
    
    uint32_t duty = angle_to_duty(angle);
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, servo_channel[servo_id], duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty for servo %d: %s", servo_id, esp_err_to_name(ret));
        return ret;
    }
    
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, servo_channel[servo_id]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty for servo %d: %s", servo_id, esp_err_to_name(ret));
        return ret;
    }
    
    current_angles[servo_id] = angle;
    ESP_LOGD(TAG, "Servo %d set to angle %lu degrees", servo_id, angle);
    
    return ESP_OK;
}

/**
 * @brief 平滑移动舵机到目标角度
 */
static esp_err_t servo_smooth_move(servo_id_t servo_id, uint32_t target_angle)
{
    uint32_t current = current_angles[servo_id];
    
    while (current != target_angle) {
        if (current < target_angle) {
            current += (target_angle - current > SMOOTH_STEP) ? SMOOTH_STEP : (target_angle - current);
        } else {
            current -= (current - target_angle > SMOOTH_STEP) ? SMOOTH_STEP : (current - target_angle);
        }
        
        esp_err_t ret = servo_set_angle(servo_id, current);
        if (ret != ESP_OK) {
            return ret;
        }
        
        vTaskDelay(pdMS_TO_TICKS(MOVE_DELAY_MS));
    }
    
    return ESP_OK;
}

esp_err_t servo_track_position(int pos_x, int pos_y, int frame_width, int frame_height)
{
    // 将图像坐标映射到舵机角度
    // X轴控制水平舵机 (SERVO_1和SERVO_2)
    // Y轴控制垂直舵机 (SERVO_3和SERVO_4)
    
    // 计算水平角度 (0-180度)
    uint32_t horizontal_angle = (pos_x * SERVO_MAX_DEGREE) / frame_width;
    if (horizontal_angle > SERVO_MAX_DEGREE) horizontal_angle = SERVO_MAX_DEGREE;
    
    // 计算垂直角度 (0-180度)
    uint32_t vertical_angle = (pos_y * SERVO_MAX_DEGREE) / frame_height;
    if (vertical_angle > SERVO_MAX_DEGREE) vertical_angle = SERVO_MAX_DEGREE;
    
    ESP_LOGI(TAG, "Tracking position (%d, %d) -> H:%lu°, V:%lu°", 
             pos_x, pos_y, horizontal_angle, vertical_angle);
    
    // 控制水平舵机 (SERVO_1和SERVO_2同步)
    esp_err_t ret = servo_smooth_move(SERVO_1, horizontal_angle);
    if (ret != ESP_OK) return ret;
    
    ret = servo_smooth_move(SERVO_2, horizontal_angle);
    if (ret != ESP_OK) return ret;
    
    // 控制垂直舵机 (SERVO_3和SERVO_4同步)
    ret = servo_smooth_move(SERVO_3, vertical_angle);
    if (ret != ESP_OK) return ret;
    
    ret = servo_smooth_move(SERVO_4, vertical_angle);
    if (ret != ESP_OK) return ret;
    
    return ESP_OK;
}

esp_err_t servo_control_stop(void)
{
    esp_err_t ret = ESP_OK;
    
    // 停止所有舵机的PWM输出
    for (int i = 0; i < SERVO_MAX; i++) {
        esp_err_t channel_ret = ledc_stop(LEDC_LOW_SPEED_MODE, servo_channel[i], 0);
        if (channel_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop servo %d: %s", i, esp_err_to_name(channel_ret));
            ret = channel_ret;
        }
    }
    
    ESP_LOGI(TAG, "All servos stopped");
    return ret;
}