/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "servo_control.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

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
static uint32_t current_angles[SERVO_MAX] = {85, 135, 85, 90}; // 初始化为各自有效范围的中位

// 舵机角度限制
static const uint32_t servo_min_angle[SERVO_MAX] = {50, 90, 50, 0};   // 最小角度
static const uint32_t servo_max_angle[SERVO_MAX] = {120, 180, 120, 180}; // 最大角度

// 平滑移动参数
#define SMOOTH_STEP 3  // 每次移动的最大角度步长
#define MOVE_DELAY_MS 10  // 移动间隔时间

// 追踪模式参数
#define SCAN_INTERVAL_MS 10000  // 扫描间隔10秒
#define TRACKING_ENABLED 1
#define TRACKING_DISABLED 0

// 滤波参数
#define FILTER_ALPHA 0.7f  // 滤波系数，越大响应越快
#define MIN_ANGLE_CHANGE 5  // 最小角度变化阈值，小于此值按5度处理

// 全局变量
static int tracking_mode = TRACKING_DISABLED;
static TimerHandle_t scan_timer = NULL;
static TaskHandle_t tracking_task_handle = NULL;
static TaskHandle_t scan_task_handle = NULL;
static int last_detected_x = -1;
static int last_detected_y = -1;

// 滤波变量
static float filtered_servo2_angle = 135.0f;  // 舵机2滤波后的角度
static float filtered_servo3_angle = 85.0f;   // 舵机3滤波后的角度

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
        uint32_t init_angle;
        
        // 舵机1特殊初始化：开机120度
        if (i == SERVO_1) {
            init_angle = 120;
        } else {
            // 其他舵机初始化为各自有效范围的中位角度
            init_angle = (servo_min_angle[i] + servo_max_angle[i]) / 2;
        }
        
        current_angles[i] = init_angle;
        
        ledc_channel_config_t ledc_channel_cfg = {
            .channel = servo_channel[i],
            .duty = angle_to_duty(init_angle),
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
    
    // 等待1秒后将舵机1移动到50度
    vTaskDelay(pdMS_TO_TICKS(1000));
    ret = servo_set_angle(SERVO_1, 50);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to move servo 1 to 50 degrees: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Servo 1 moved from 120° to 50° after initialization");
    
    ESP_LOGI(TAG, "Servo control initialized successfully");
    return ESP_OK;
}

esp_err_t servo_set_angle(servo_id_t servo_id, uint32_t angle)
{
    if (servo_id >= SERVO_MAX) {
        ESP_LOGE(TAG, "Invalid servo ID: %d", servo_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 应用舵机特定的角度限制
    if (angle < servo_min_angle[servo_id]) {
        ESP_LOGW(TAG, "Servo %d angle %lu below minimum, clamping to %lu", servo_id, angle, servo_min_angle[servo_id]);
        angle = servo_min_angle[servo_id];
    }
    if (angle > servo_max_angle[servo_id]) {
        ESP_LOGW(TAG, "Servo %d angle %lu exceeds maximum, clamping to %lu", servo_id, angle, servo_max_angle[servo_id]);
        angle = servo_max_angle[servo_id];
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
    // 只在追踪模式下执行
    if (tracking_mode != TRACKING_ENABLED) {
        return ESP_OK;
    }
    
    // 更新最后检测到的位置
    last_detected_x = pos_x;
    last_detected_y = pos_y;
    
    // 计算目标角度
    float target_servo2_angle = (float)(servo_min_angle[SERVO_2] + 
        (pos_x * (servo_max_angle[SERVO_2] - servo_min_angle[SERVO_2])) / frame_width);
    
    float target_servo3_angle = (float)(servo_min_angle[SERVO_3] + 
        (pos_y * (servo_max_angle[SERVO_3] - servo_min_angle[SERVO_3])) / frame_height);
    
    // 应用低通滤波
    filtered_servo2_angle = FILTER_ALPHA * target_servo2_angle + (1.0f - FILTER_ALPHA) * filtered_servo2_angle;
    filtered_servo3_angle = FILTER_ALPHA * target_servo3_angle + (1.0f - FILTER_ALPHA) * filtered_servo3_angle;
    
    // 计算角度变化并处理小幅度动作
    uint32_t new_servo2_angle = (uint32_t)(filtered_servo2_angle + 0.5f);  // 四舍五入
    uint32_t new_servo3_angle = (uint32_t)(filtered_servo3_angle + 0.5f);  // 四舍五入
    
    int angle_diff_servo2 = (int)new_servo2_angle - (int)current_angles[SERVO_2];
    int angle_diff_servo3 = (int)new_servo3_angle - (int)current_angles[SERVO_3];
    
    // 对于不足MIN_ANGLE_CHANGE的动作，按MIN_ANGLE_CHANGE的方向处理
    uint32_t final_servo2_angle = current_angles[SERVO_2];
    uint32_t final_servo3_angle = current_angles[SERVO_3];
    
    bool need_move_servo2 = false;
    bool need_move_servo3 = false;
    
    if (abs(angle_diff_servo2) >= MIN_ANGLE_CHANGE) {
        final_servo2_angle = new_servo2_angle;
        need_move_servo2 = true;
    } else if (angle_diff_servo2 != 0) {
        // 不足5度的动作按5度处理
        final_servo2_angle = current_angles[SERVO_2] + (angle_diff_servo2 > 0 ? MIN_ANGLE_CHANGE : -MIN_ANGLE_CHANGE);
        need_move_servo2 = true;
    }
    
    if (abs(angle_diff_servo3) >= MIN_ANGLE_CHANGE) {
        final_servo3_angle = new_servo3_angle;
        need_move_servo3 = true;
    } else if (angle_diff_servo3 != 0) {
        // 不足5度的动作按5度处理
        final_servo3_angle = current_angles[SERVO_3] + (angle_diff_servo3 > 0 ? MIN_ANGLE_CHANGE : -MIN_ANGLE_CHANGE);
        need_move_servo3 = true;
    }
    
    ESP_LOGI(TAG, "Tracking position (%d, %d) -> Target S2:%.1f°, S3:%.1f° -> Final S2:%lu°, S3:%lu°", 
             pos_x, pos_y, target_servo2_angle, target_servo3_angle, final_servo2_angle, final_servo3_angle);
    
    // 执行舵机移动
    esp_err_t ret = ESP_OK;
    if (need_move_servo2) {
        ret = servo_smooth_move(SERVO_2, final_servo2_angle);
        if (ret != ESP_OK) return ret;
    }
    
    if (need_move_servo3) {
        ret = servo_smooth_move(SERVO_3, final_servo3_angle);
        if (ret != ESP_OK) return ret;
    }
    
    return ESP_OK;
}

esp_err_t servo_control_stop(void)
{
    esp_err_t ret = ESP_OK;
    
    // 停止追踪模式
    servo_stop_tracking();
    
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

/**
 * @brief 扫描任务函数
 */
static void scan_task(void *pvParameters)
{
    while (1) {
        // 等待任务通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // 执行扫描动作
        servo_scan_action();
    }
}

/**
 * @brief 扫描定时器回调函数
 */
static void scan_timer_callback(TimerHandle_t xTimer)
{
    // 通过任务通知触发扫描动作，避免在定时器回调中执行耗时操作
    if (scan_task_handle != NULL) {
        xTaskNotifyGive(scan_task_handle);
    }
}

esp_err_t servo_scan_action(void)
{
    // 随机选择三种动作之一：0=害怕动作, 1=赞同动作, 2=原有随机动作
    uint32_t action_type = esp_random() % 3;
    esp_err_t ret = ESP_OK;
    
    switch (action_type) {
        case 0: // 害怕动作：舵机3左右最小10度动作，伴随舵机1和3小幅度抖动
            ESP_LOGI(TAG, "Executing fear action (servo 3 left-right 10° min, with servo 1&3 twitching)");
            {
                uint32_t base_angle_servo3 = 85;
                uint32_t base_angle_servo1 = 85; // 舵机1的基准角度
                uint32_t start_time = xTaskGetTickCount();
                bool direction = true; // true=右移, false=左移
                
                // 持续3秒的害怕动作
                while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(3000)) {
                    // 舵机3左右移动，最小10度
                    uint32_t move_angle = 10 + (esp_random() % 6); // 10-15度随机移动
                    uint32_t target_angle_servo3;
                    
                    if (direction) {
                        target_angle_servo3 = base_angle_servo3 + move_angle; // 向右
                    } else {
                        target_angle_servo3 = base_angle_servo3 - move_angle; // 向左
                    }
                    direction = !direction; // 切换方向
                    
                    // 确保舵机3角度在有效范围内
                    if (target_angle_servo3 < servo_min_angle[SERVO_3]) target_angle_servo3 = servo_min_angle[SERVO_3];
                    if (target_angle_servo3 > servo_max_angle[SERVO_3]) target_angle_servo3 = servo_max_angle[SERVO_3];
                    
                    // 舵机1小幅度抖动
                    uint32_t twitch_angle_servo1 = base_angle_servo1 + (esp_random() % 7) - 3; // ±3度抖动
                    if (twitch_angle_servo1 < servo_min_angle[SERVO_1]) twitch_angle_servo1 = servo_min_angle[SERVO_1];
                    if (twitch_angle_servo1 > servo_max_angle[SERVO_1]) twitch_angle_servo1 = servo_max_angle[SERVO_1];
                    
                    // 舵机3小幅度抖动（在目标角度基础上）
                    uint32_t twitch_offset = (esp_random() % 5) - 2; // ±2度抖动
                    uint32_t final_angle_servo3 = target_angle_servo3 + twitch_offset;
                    if (final_angle_servo3 < servo_min_angle[SERVO_3]) final_angle_servo3 = servo_min_angle[SERVO_3];
                    if (final_angle_servo3 > servo_max_angle[SERVO_3]) final_angle_servo3 = servo_max_angle[SERVO_3];
                    
                    // 同时移动舵机1和3
                    ret = servo_set_angle(SERVO_1, twitch_angle_servo1);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to move servo 1 to %lu°", twitch_angle_servo1);
                    }
                    
                    ret = servo_set_angle(SERVO_3, final_angle_servo3);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to move servo 3 to %lu°", final_angle_servo3);
                        break;
                    }
                    
                    vTaskDelay(pdMS_TO_TICKS(150)); // 每150ms执行一次动作
                }
                ESP_LOGI(TAG, "Fear action completed");
            }
            break;
            
        case 1: // 赞同动作：舵机2在120-150度先快后慢
            ESP_LOGI(TAG, "Executing approval action (servo 2 fast then slow 120-150°)");
            {
                uint32_t start_angle = 120;
                uint32_t end_angle = 150;
                
                // 快速移动到150度
                ret = servo_set_angle(SERVO_2, end_angle);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to move servo 2 to %lu°", end_angle);
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(500)); // 停留0.5秒
                
                // 慢速移动回120度
                ret = servo_smooth_move(SERVO_2, start_angle);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to move servo 2 to %lu°", start_angle);
                    break;
                }
                
                // 再次快速到150度
                ret = servo_set_angle(SERVO_2, end_angle);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to move servo 2 to %lu°", end_angle);
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(500)); // 停留0.5秒
                
                // 最后慢速回到中间位置
                ret = servo_smooth_move(SERVO_2, 135);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to move servo 2 to 135°");
                    break;
                }
                
                ESP_LOGI(TAG, "Approval action completed");
            }
            break;
            
        case 2: // 原有随机动作：舵机1随机移动
        default:
            ESP_LOGI(TAG, "Executing random scan action (servo 1)");
            {
                uint32_t min_angle = servo_min_angle[SERVO_1]; // 50度
                uint32_t max_angle = servo_max_angle[SERVO_1]; // 120度
                uint32_t start_time = xTaskGetTickCount();
                
                // 持续3秒的随机动作
                while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(3000)) {
                    uint32_t random_angle = min_angle + (esp_random() % (max_angle - min_angle + 1));
                    
                    ret = servo_smooth_move(SERVO_1, random_angle);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to move servo 1 to %lu°", random_angle);
                        break;
                    }
                    
                    ESP_LOGI(TAG, "Moved to random angle: %lu°", random_angle);
                    vTaskDelay(pdMS_TO_TICKS(800)); // 停留0.8秒
                }
                ESP_LOGI(TAG, "Random scan action completed");
            }
            break;
    }
    
    return ret;
}

esp_err_t servo_start_tracking(void)
{
    if (tracking_mode == TRACKING_ENABLED) {
        ESP_LOGW(TAG, "Tracking mode already enabled");
        return ESP_OK;
    }
    
    tracking_mode = TRACKING_ENABLED;
    
    // 初始化滤波变量为当前舵机角度
    filtered_servo2_angle = (float)current_angles[SERVO_2];
    filtered_servo3_angle = (float)current_angles[SERVO_3];
    
    // 创建扫描任务
    if (scan_task_handle == NULL) {
        BaseType_t task_ret = xTaskCreate(scan_task, "ScanTask", 4096, NULL, 5, &scan_task_handle);
        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create scan task");
            tracking_mode = TRACKING_DISABLED;
            return ESP_FAIL;
        }
    }
    
    // 创建扫描定时器
    if (scan_timer == NULL) {
        scan_timer = xTimerCreate("ScanTimer", 
                                 pdMS_TO_TICKS(SCAN_INTERVAL_MS),
                                 pdTRUE, // 自动重载
                                 NULL,
                                 scan_timer_callback);
        
        if (scan_timer == NULL) {
            ESP_LOGE(TAG, "Failed to create scan timer");
            tracking_mode = TRACKING_DISABLED;
            return ESP_FAIL;
        }
    }
    
    // 启动定时器
    if (xTimerStart(scan_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start scan timer");
        tracking_mode = TRACKING_DISABLED;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Tracking mode started with 10s scan interval");
    return ESP_OK;
}

esp_err_t servo_stop_tracking(void)
{
    if (tracking_mode == TRACKING_DISABLED) {
        ESP_LOGW(TAG, "Tracking mode already disabled");
        return ESP_OK;
    }
    
    tracking_mode = TRACKING_DISABLED;
    
    // 停止定时器
    if (scan_timer != NULL) {
        xTimerStop(scan_timer, 0);
        xTimerDelete(scan_timer, 0);
        scan_timer = NULL;
    }
    
    // 删除扫描任务
    if (scan_task_handle != NULL) {
        vTaskDelete(scan_task_handle);
        scan_task_handle = NULL;
    }
    
    // 重置位置记录
    last_detected_x = -1;
    last_detected_y = -1;
    
    ESP_LOGI(TAG, "Tracking mode stopped");
    return ESP_OK;
}