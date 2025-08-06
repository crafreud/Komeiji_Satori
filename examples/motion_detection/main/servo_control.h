/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SERVO_1_PIN     GPIO_NUM_1
#define SERVO_2_PIN     GPIO_NUM_2
#define SERVO_3_PIN     GPIO_NUM_4
#define SERVO_4_PIN     GPIO_NUM_10

// 舵机PWM参数
#define SERVO_MIN_PULSEWIDTH_US 500   // 最小脉宽 (0度)
#define SERVO_MAX_PULSEWIDTH_US 2500  // 最大脉宽 (180度)
#define SERVO_MAX_DEGREE        180   // 最大角度
#define SERVO_FREQ_HZ           50    // PWM频率 50Hz

// 舵机编号枚举
typedef enum {
    SERVO_1 = 0,
    SERVO_2 = 1,
    SERVO_3 = 2,
    SERVO_4 = 3,
    SERVO_MAX
} servo_id_t;

/**
 * @brief 初始化舵机控制系统
 * 
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t servo_control_init(void);

/**
 * @brief 设置指定舵机的角度
 * 
 * @param servo_id 舵机编号 (0-3)
 * @param angle 目标角度 (0-180度)
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t servo_set_angle(servo_id_t servo_id, uint32_t angle);

/**
 * @brief 根据检测到的坐标控制舵机移动
 * 
 * @param pos_x 检测到的X坐标
 * @param pos_y 检测到的Y坐标
 * @param frame_width 图像帧宽度
 * @param frame_height 图像帧高度
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t servo_track_position(int pos_x, int pos_y, int frame_width, int frame_height);

/**
 * @brief 停止所有舵机
 * 
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t servo_control_stop(void);

#ifdef __cplusplus
}
#endif

#endif // SERVO_CONTROL_H