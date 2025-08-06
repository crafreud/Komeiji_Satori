#ifndef SERVO_H
#define SERVO_H

#include "driver/ledc.h"
#include "esp_err.h"

// 舵机引脚宏定义
#define SERVO_1_PIN     GPIO_NUM_1
#define SERVO_2_PIN     GPIO_NUM_2
#define SERVO_3_PIN     GPIO_NUM_4
#define SERVO_4_PIN     GPIO_NUM_10

// 舵机PWM参数宏定义
#define SERVO_PWM_FREQUENCY     50      // 50Hz频率
#define SERVO_PWM_FREQ_HZ       50      // 50Hz频率
#define SERVO_PWM_RESOLUTION    LEDC_TIMER_14_BIT
#define SERVO_PWM_TIMER         LEDC_TIMER_0
#define SERVO_PWM_MODE          LEDC_LOW_SPEED_MODE

// 舵机脉宽参数
#define SERVO_MIN_PULSEWIDTH_US 500     // 最小脉宽500us (0度)
#define SERVO_MAX_PULSEWIDTH_US 2500    // 最大脉宽2500us (180度)
#define SERVO_PWM_PERIOD_US     20000   // PWM周期20ms = 20000us
#define SERVO_MAX_DEGREE        180     // 最大角度180度

// 舵机通道定义
#define SERVO_1_CHANNEL     LEDC_CHANNEL_0
#define SERVO_2_CHANNEL     LEDC_CHANNEL_1
#define SERVO_3_CHANNEL     LEDC_CHANNEL_2
#define SERVO_4_CHANNEL     LEDC_CHANNEL_3

// 舵机角度转换宏（0-180度对应的占空比）
#define SERVO_MIN_DUTY      410     // 0.5ms / 20ms * 16384 ≈ 410 (0度)
#define SERVO_MAX_DUTY      2048    // 2.5ms / 20ms * 16384 ≈ 2048 (180度)
#define SERVO_MID_DUTY      1229    // 1.5ms / 20ms * 16384 ≈ 1229 (90度)

// 角度转占空比计算宏
#define ANGLE_TO_DUTY(angle) (SERVO_MIN_DUTY + (angle * (SERVO_MAX_DUTY - SERVO_MIN_DUTY) / 180))

/**
 * @brief 初始化舵机PWM
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t servo_init(void);

/**
 * @brief 设置舵机角度
 * @param servo_channel 舵机通道 (SERVO_1_CHANNEL ~ SERVO_4_CHANNEL)
 * @param angle 角度 (0-180度)
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t servo_set_angle(ledc_channel_t servo_channel, int angle);

/**
 * @brief 舵机测试函数
 */
void servo_test(void);

/**
 * @brief 设置所有舵机到指定角度
 * @param angle 角度 (0-180度)
 */
esp_err_t servo_set_all_angle(int angle);

#endif // SERVO_H