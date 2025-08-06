#include "servo.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

/**
 * @brief 初始化舵机PWM
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t servo_init(void)
{
    printf("开始初始化舵机PWM...\n");
    
    // 配置PWM定时器
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = SERVO_PWM_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        printf("PWM定时器配置失败: %s\n", esp_err_to_name(ret));
        return ret;
    }
    printf("PWM定时器配置成功: 频率%dHz, 分辨率14位\n", SERVO_PWM_FREQUENCY);

    // 配置舵机1通道
    ledc_channel_config_t servo1_conf = {
        .gpio_num = SERVO_1_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = SERVO_1_CHANNEL,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0
    };
    ret = ledc_channel_config(&servo1_conf);
    if (ret != ESP_OK) {
        printf("舵机1通道配置失败: %s\n", esp_err_to_name(ret));
        return ret;
    }
    printf("舵机1通道配置成功: GPIO%d, 通道%d\n", SERVO_1_PIN, SERVO_1_CHANNEL);

    // 配置舵机2通道
    ledc_channel_config_t servo2_conf = {
        .gpio_num = SERVO_2_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = SERVO_2_CHANNEL,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0
    };
    ret = ledc_channel_config(&servo2_conf);
    if (ret != ESP_OK) {
        printf("舵机2通道配置失败: %s\n", esp_err_to_name(ret));
        return ret;
    }
    printf("舵机2通道配置成功: GPIO%d, 通道%d\n", SERVO_2_PIN, SERVO_2_CHANNEL);

    // 配置舵机3通道
    ledc_channel_config_t servo3_conf = {
        .gpio_num = SERVO_3_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = SERVO_3_CHANNEL,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0
    };
    ret = ledc_channel_config(&servo3_conf);
    if (ret != ESP_OK) {
        printf("舵机3通道配置失败: %s\n", esp_err_to_name(ret));
        return ret;
    }
    printf("舵机3通道配置成功: GPIO%d, 通道%d\n", SERVO_3_PIN, SERVO_3_CHANNEL);

    // 配置舵机4通道
    ledc_channel_config_t servo4_conf = {
        .gpio_num = SERVO_4_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = SERVO_4_CHANNEL,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0
    };
    ret = ledc_channel_config(&servo4_conf);
    if (ret != ESP_OK) {
        printf("舵机4通道配置失败: %s\n", esp_err_to_name(ret));
        return ret;
    }
    printf("舵机4通道配置成功: GPIO%d, 通道%d\n", SERVO_4_PIN, SERVO_4_CHANNEL);

    // 初始化阶段：将每个舵机从0度转到最大角度180度
    printf("开始舵机初始化转动：从0度转到最大角度180度\n");
    
    // 先将所有舵机设置到0度
    printf("设置所有舵机到0度\n");
    servo_set_all_angle(0);
    vTaskDelay(pdMS_TO_TICKS(1000));  // 等待1秒
    
    // 然后将所有舵机转到最大角度180度
    printf("设置所有舵机到最大角度180度\n");
    servo_set_all_angle(180);
    vTaskDelay(pdMS_TO_TICKS(1000));  // 等待1秒
    
    printf("舵机初始化转动完成\n");
    printf("舵机初始化完成\n");
    return ESP_OK;
}

// 角度转换为脉宽微秒数
static uint32_t angle_to_duty_us(uint32_t angle)
{
    uint32_t pulsewidth = SERVO_MIN_PULSEWIDTH_US + ((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * angle) / SERVO_MAX_DEGREE;
    return pulsewidth;
}

/**
 * @brief 设置舵机角度
 * @param servo_channel 舵机通道 (SERVO_1_CHANNEL ~ SERVO_4_CHANNEL)
 * @param angle 角度 (0-180度)
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t servo_set_angle(ledc_channel_t servo_channel, int angle)
{
    if (angle < 0 || angle > 180) {
        printf("角度超出范围: %d (应在0-180之间)\n", angle);
        return ESP_ERR_INVALID_ARG;
    }
    
    uint32_t duty_us = angle_to_duty_us((uint32_t)angle);
    uint32_t duty = (duty_us * ((1 << 14) - 1)) / SERVO_PWM_PERIOD_US;
    
    printf("舵机通道%d: 角度%d度 -> 脉宽%luus -> 占空比%lu\n", 
           servo_channel, angle, duty_us, duty);
    
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, servo_channel, duty);
    if (ret != ESP_OK) {
        printf("设置舵机占空比失败: %s\n", esp_err_to_name(ret));
        return ret;
    }
    
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, servo_channel);
    if (ret != ESP_OK) {
        printf("更新舵机占空比失败: %s\n", esp_err_to_name(ret));
        return ret;
    }
    
    printf("舵机通道%d设置完成: %d度\n", servo_channel, angle);
    return ESP_OK;
}

/**
 * @brief 设置所有舵机到指定角度
 * @param angle 角度 (0-180度)
 */
esp_err_t servo_set_all_angle(int angle)
{
    esp_err_t ret = ESP_OK;
    
    ret |= servo_set_angle(SERVO_1_CHANNEL, angle);
    ret |= servo_set_angle(SERVO_2_CHANNEL, angle);
    ret |= servo_set_angle(SERVO_3_CHANNEL, angle);
    ret |= servo_set_angle(SERVO_4_CHANNEL, angle);
    
    return ret;
}

/**
 * @brief 舵机测试函数
 */
void servo_test(void)
{
    printf("开始舵机测试...\n");
    
    // 所有舵机转到0度
    printf("所有舵机转到0度\n");
    servo_set_all_angle(0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 所有舵机转到90度
    printf("所有舵机转到90度\n");
    servo_set_all_angle(90);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 所有舵机转到180度
    printf("所有舵机转到180度\n");
    servo_set_all_angle(180);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 回到中位
    printf("所有舵机回到中位(90度)\n");
    servo_set_all_angle(90);
    
    printf("舵机测试完成\n");
}