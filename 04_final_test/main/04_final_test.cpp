#include <stdio.h>
#include <string.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "who_human_face_detection.hpp"
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "who_ai_utils.hpp"
#include "dl_image.hpp"
#include <list> // ✅ std::list
#include <algorithm> // 用于std::abs
#include <vector>

#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <list>
#include "who_human_face_detection.hpp"
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "who_ai_utils.hpp"
#include "dl_image.hpp"

static const char *TAG = "face_servo";

// ---------------- 工具函数 ----------------
// 限制数值在指定范围内
int constrain(int value, int min_val, int max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

// ---------------- 舵机配置 ----------------
#define SERVO1_PIN 1
#define SERVO2_PIN 2
#define SERVO3_PIN 4
#define SERVO4_PIN 10

#define SERVO_MIN_US 500 // 微秒
#define SERVO_MAX_US 2500
#define SERVO_FREQ 50 // 50Hz

// ---------------- 移动检测配置 ----------------
#define MOTION_THRESHOLD 10     // 像素差异阈值 (降低以提高敏感度)
#define MOTION_MIN_AREA 200     // 最小移动区域像素数 (降低以检测更小的移动)
#define MOTION_DETECT_INTERVAL 20 // 每隔几帧检测一次移动 (降低检测频率)

// // ---------------- 摄像头引脚定义 ----------------
#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 17
#define CAM_PIN_SIOD 20
#define CAM_PIN_SIOC 19
#define CAM_PIN_D7 42
#define CAM_PIN_D6 41
#define CAM_PIN_D5 40
#define CAM_PIN_D4 39
#define CAM_PIN_D3 38
#define CAM_PIN_D2 13
#define CAM_PIN_D1 12
#define CAM_PIN_D0 11
#define CAM_PIN_VSYNC 8
#define CAM_PIN_HREF 18
#define CAM_PIN_PCLK 16

// 初始化 PWM
void servo_init()
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = SERVO_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false};
    ledc_timer_config(&timer_conf);

    int pins[4] = {SERVO1_PIN, SERVO2_PIN, SERVO3_PIN, SERVO4_PIN};
    for (int i = 0; i < 4; i++)
    {
        ledc_channel_config_t ch_conf = {
            .gpio_num = pins[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t)i,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
            .flags = {0}};
        ledc_channel_config(&ch_conf);
    }
}

// 设置舵机角度
void servo_set_angle(int id, int angle)
{
    if (id < 1 || id > 4)
        return;
    int duty = SERVO_MIN_US + (angle * 1.0f / 180) * (SERVO_MAX_US - SERVO_MIN_US);
    // 转换为 16-bit duty
    int duty16 = (duty * (1 << 14) / 20000); // 50Hz -> 20ms
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)(id - 1), duty16);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)(id - 1));
    
    ESP_LOGI(TAG, "Servo %d: angle=%d°, duty_us=%d, duty16=%d", id, angle, duty, duty16);
}

// 打印所有舵机的PWM状态
void servo_print_all_pwm()
{
    ESP_LOGI(TAG, "=== 舵机PWM状态 ===");
    for (int i = 0; i < 4; i++) {
        uint32_t duty = ledc_get_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i);
        // 将duty转换回角度进行验证
        int duty_us = (duty * 20000) / (1 << 14);
        int angle = ((duty_us - SERVO_MIN_US) * 180) / (SERVO_MAX_US - SERVO_MIN_US);
        ESP_LOGI(TAG, "Servo %d: duty16=%lu, duty_us=%d, angle=%d°", i+1, duty, duty_us, angle);
    }
    ESP_LOGI(TAG, "==================");
}

// ---------------- 人脸检测任务 ----------------
// 舵机角度范围
#define SERVO_H_MIN 50
#define SERVO_H_MAX 120
#define SERVO_V_MIN 100
#define SERVO_V_MAX 130
#define SERVO_1_MIN 30
#define SERVO_1_MAX 100

// void task_face_detect(void *arg)
// {
//     HumanFaceDetectMSR01 detector(0.3F, 0.3F, 10, 0.3F);
//     HumanFaceDetectMNP01 detector2(0.4F, 0.3F, 10);

//     int servo_h_angle = 90; // 初始水平角
//     int servo_v_angle = 70; // 初始垂直角

//     while (1)
//     {
//         camera_fb_t *fb = esp_camera_fb_get();
//         if (!fb)
//         {
//             vTaskDelay(pdMS_TO_TICKS(100));
//             continue;
//         }

//         std::list<dl::detect::result_t> &candidates = detector.infer(
//             (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3});
//         std::list<dl::detect::result_t> &results = detector2.infer(
//             (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3}, candidates);

//         if (!results.empty())
//         {
//             auto face = results.front(); // 取第一个人脸
//             int xmin = face.box[0];
//             int ymin = face.box[1];
//             int xmax = face.box[2];
//             int ymax = face.box[3];

//             int cx = (xmin + xmax) / 2;
//             int cy = (ymin + ymax) / 2;

//             int img_w = fb->width;
//             int img_h = fb->height;

//             // ---------------- 水平控制（3号舵机） ----------------
//             float ratio_h = 1.0f - (float)cx / img_w; // 左右翻转
//             int servo_h_angle = SERVO_H_MIN + ratio_h * (SERVO_H_MAX - SERVO_H_MIN);
//             servo_set_angle(3, servo_h_angle);

//             // ---------------- 垂直控制（2号舵机） ----------------
//             float ratio_v = (float)cy / img_h; // 上下映射
//             int servo_v_angle = SERVO_V_MIN + ratio_v * (SERVO_V_MAX - SERVO_V_MIN);
//             servo_set_angle(2, servo_v_angle);

//             ESP_LOGI(TAG, "Face center: (%d,%d) -> servo H: %d, V: %d", cx, cy, servo_h_angle, servo_v_angle);
//         }

//         esp_camera_fb_return(fb);
//         vTaskDelay(pdMS_TO_TICKS(50));
//     }
// }
// ---------------- 舵机角度范围 ----------------
#define SERVO_H_MIN 50
#define SERVO_H_MAX 120
#define SERVO_V_MIN 100
#define SERVO_V_MAX 130
#define SERVO_1_MIN 30
#define SERVO_1_MAX 100

// static const char *TAG = "face_pid";

// ---------------- 简单 PID 结构体 ----------------
typedef struct {
    float Kp;
    float Ki;
    float Kd;
    float prev_error;
    float integral;
} PID_t;

// ---------------- 移动检测结构体 ----------------
typedef struct {
    uint16_t* prev_frame;  // 上一帧图像
    int width;             // 图像宽度
    int height;            // 图像高度
    int frame_count;       // 帧计数器
    bool has_motion;       // 是否检测到移动
    int motion_cx;         // 移动物体中心x坐标
    int motion_cy;         // 移动物体中心y坐标
} MotionDetect_t;

// --------------- 目标仲裁类型与结果结构 ----------------
enum TargetType { TARGET_NONE, TARGET_FACE, TARGET_MOTION };
typedef struct {
    bool detected;
    TargetType type;
    int cx;
    int cy;
} TargetInfo;

// ---------------- PID 计算函数 ----------------
float pid_compute(PID_t* pid, float setpoint, float measurement)
{
    float error = setpoint - measurement;
    pid->integral += error;
    float derivative = error - pid->prev_error;
    pid->prev_error = error;
    float output = pid->Kp * error + pid->Ki * pid->integral + pid->Kd * derivative;
    return output;
}

// ---------------- 移动检测初始化 ----------------
MotionDetect_t* motion_detect_init(int width, int height)
{
    MotionDetect_t* md = (MotionDetect_t*)malloc(sizeof(MotionDetect_t));
    if (!md) return NULL;
    
    md->width = width;
    md->height = height;
    md->prev_frame = (uint16_t*)malloc(width * height * sizeof(uint16_t));
    if (!md->prev_frame) {
        free(md);
        return NULL;
    }
    
    memset(md->prev_frame, 0, width * height * sizeof(uint16_t));
    md->frame_count = 0;
    md->has_motion = false;
    md->motion_cx = width / 2;
    md->motion_cy = height / 2;
    
    return md;
}

// ---------------- 移动检测释放 ----------------
void motion_detect_free(MotionDetect_t* md)
{
    if (md) {
        if (md->prev_frame) free(md->prev_frame);
        free(md);
    }
}

// ---------------- 移动检测处理 ----------------
bool motion_detect_process(MotionDetect_t* md, uint16_t* current_frame)
{
    // 每隔几帧检测一次移动
    md->frame_count++;
    if (md->frame_count % MOTION_DETECT_INTERVAL != 0) {
        return md->has_motion;
    }
    
    int diff_count = 0;
    int sum_x = 0;
    int sum_y = 0;
    
    // 计算帧差并找出移动区域中心
    for (int y = 0; y < md->height; y++) {
        for (int x = 0; x < md->width; x++) {
            int idx = y * md->width + x;
            
            // RGB565格式，提取亮度信息
            uint16_t curr_pixel = current_frame[idx];
            uint16_t prev_pixel = md->prev_frame[idx];
            
            // 简单的亮度差异计算
            uint8_t curr_r = (curr_pixel >> 11) & 0x1F;
            uint8_t curr_g = (curr_pixel >> 5) & 0x3F;
            uint8_t curr_b = curr_pixel & 0x1F;
            uint8_t prev_r = (prev_pixel >> 11) & 0x1F;
            uint8_t prev_g = (prev_pixel >> 5) & 0x3F;
            uint8_t prev_b = prev_pixel & 0x1F;
            
            int diff = abs(curr_r - prev_r) + abs(curr_g - prev_g) + abs(curr_b - prev_b);
            
            if (diff > MOTION_THRESHOLD) {
                diff_count++;
                sum_x += x;
                sum_y += y;
            }
            
            // 更新上一帧
            md->prev_frame[idx] = curr_pixel;
        }
    }
    
    // 判断是否有足够大的移动区域
    if (diff_count > MOTION_MIN_AREA) {
        md->has_motion = true;
        md->motion_cx = sum_x / diff_count;
        md->motion_cy = sum_y / diff_count;
    } else {
        md->has_motion = false;
    }
    
    return md->has_motion;
}

// ---------------- 舵机控制函数接口 ----------------
// 这里假设你已有函数：void servo_set_angle(int servo_id, int angle);

// --------------- 目标仲裁与日志函数 ----------------
static TargetInfo arbitrate_target(const std::list<dl::detect::result_t>& face_results,
                                  MotionDetect_t* motion_detector,
                                  uint16_t* frame_buf)
{
    TargetInfo info{false, TARGET_NONE, 0, 0};
    if (!face_results.empty())
    {
        const auto& face = face_results.front();
        int xmin = face.box[0];
        int ymin = face.box[1];
        int xmax = face.box[2];
        int ymax = face.box[3];
        info.cx = (xmin + xmax) / 2;
        info.cy = (ymin + ymax) / 2;
        info.detected = true;
        info.type = TARGET_FACE;
        return info;
    }
    if (motion_detector)
    {
        bool has_motion = motion_detect_process(motion_detector, frame_buf);
        if (has_motion) {
            info.detected = true;
            info.type = TARGET_MOTION;
            info.cx = motion_detector->motion_cx;
            info.cy = motion_detector->motion_cy;
        }
    }
    return info;
}

static inline void log_target(const TargetInfo& t)
{
    if (!t.detected) {
        ESP_LOGI(TAG, "No target detected");
    } else if (t.type == TARGET_FACE) {
        ESP_LOGI(TAG, "Face detected at: (%d,%d)", t.cx, t.cy);
    } else {
        ESP_LOGI(TAG, "Motion detected at: (%d,%d)", t.cx, t.cy);
    }
}

// 全局变量声明 (在函数外部)
// 全局舵机角度变量
int current_servo_h = 110; // 水平初始角度110度
int current_servo_v = 115; // 垂直初始角度115度
int current_servo_1 = 60; // 1号舵机初始角度60度

// 日志控制变量
static int last_logged_servo_h = -1;
static int last_logged_servo_v = -1;
static int last_logged_servo_1 = -1;
static int64_t last_log_time = 0;
static const int64_t LOG_INTERVAL_US = 2000000; // 2秒间隔 (微秒)

// 速度控制枚举
enum SpeedMode {
    SPEED_VERY_SLOW = 400,
    SPEED_SLOW = 300,
    SPEED_NORMAL = 200,
    SPEED_FAST = 100,
    SPEED_VERY_FAST = 50
};

// 随机动作函数声明
void perform_horizontal_swing(int delay_ms);
void perform_vertical_swing(int delay_ms);
void perform_servo1_action(int delay_ms);
void perform_wave_action(int delay_ms);
void perform_circle_scan(int delay_ms);
void perform_vibration_mode(int delay_ms);
void perform_focus_action(int delay_ms);
void perform_star_trajectory(int delay_ms);
void perform_search_mode(int delay_ms);
void perform_spiral_action(int delay_ms);
// 新增10种随机动作函数声明
void perform_zigzag_action(int delay_ms);
void perform_figure_eight(int delay_ms);
void perform_pendulum_action(int delay_ms);
void perform_random_walk(int delay_ms);
void perform_pulse_action(int delay_ms);
void perform_smooth_curve(int delay_ms);
void perform_corner_scan(int delay_ms);
void perform_cross_pattern(int delay_ms);
void perform_diamond_scan(int delay_ms);
void perform_breathing_action(int delay_ms);

// 随机动作系统 - 扩展为24种动作组合
void perform_random_action_sequence()
{
    // 随机选择动作组合 (1-24)
    int action_combo = 1 + (esp_random() % 24);
    
    // 随机选择速度模式 (5种速度)
    SpeedMode speed_modes[] = {SPEED_VERY_SLOW, SPEED_SLOW, SPEED_NORMAL, SPEED_FAST, SPEED_VERY_FAST};
    SpeedMode selected_speed = speed_modes[esp_random() % 5];
    int delay_ms = (int)selected_speed;
    
    const char* speed_names[] = {"超慢速", "慢速", "正常", "快速", "超快速"};
    int speed_index = 0;
    for (int i = 0; i < 5; i++) {
        if (speed_modes[i] == selected_speed) {
            speed_index = i;
            break;
        }
    }
    
    ESP_LOGI(TAG, "🎭 执行随机动作组合 %d (%s)", action_combo, speed_names[speed_index]);
    
    // 记录初始位置
    int initial_h = 110;  // 水平舵机初始位置
    int initial_v = 115;  // 垂直舵机初始位置  
    int initial_1 = 60;   // 1号舵机初始位置
    
    // 执行对应的动作组合
    switch(action_combo) {
        case 1: // 仅水平摆动
            perform_horizontal_swing(delay_ms);
            break;
        case 2: // 仅垂直摆动
            perform_vertical_swing(delay_ms);
            break;
        case 3: // 仅1号舵机到60度
            perform_servo1_action(delay_ms);
            break;
        case 4: // 水平摆动 + 垂直摆动
            perform_horizontal_swing(delay_ms);
            perform_vertical_swing(delay_ms);
            break;
        case 5: // 水平摆动 + 1号舵机
            perform_horizontal_swing(delay_ms);
            perform_servo1_action(delay_ms);
            break;
        case 6: // 垂直摆动 + 1号舵机
            perform_vertical_swing(delay_ms);
            perform_servo1_action(delay_ms);
            break;
        case 7: // 全部动作
            perform_horizontal_swing(delay_ms);
            perform_vertical_swing(delay_ms);
            perform_servo1_action(delay_ms);
            break;
        // 原有7种动作
        case 8: // 🌊 波浪动作
            perform_wave_action(delay_ms);
            break;
        case 9: // 🔄 圆形扫描
            perform_circle_scan(delay_ms);
            break;
        case 10: // ⚡ 震动模式
            perform_vibration_mode(delay_ms);
            break;
        case 11: // 🎯 聚焦动作
            perform_focus_action(delay_ms);
            break;
        case 12: // 🌟 星形轨迹
            perform_star_trajectory(delay_ms);
            break;
        case 13: // 🔍 搜索模式
            perform_search_mode(delay_ms);
            break;
        case 14: // 💫 螺旋动作
            perform_spiral_action(delay_ms);
            break;
        // 新增10种动作
        case 15: // ⚡ Z字形扫描
            perform_zigzag_action(delay_ms);
            break;
        case 16: // ∞ 8字形轨迹
            perform_figure_eight(delay_ms);
            break;
        case 17: // 🕰️ 钟摆动作
            perform_pendulum_action(delay_ms);
            break;
        case 18: // 🚶 随机游走
            perform_random_walk(delay_ms);
            break;
        case 19: // 💓 脉冲动作
            perform_pulse_action(delay_ms);
            break;
        case 20: // 🌊 平滑曲线
            perform_smooth_curve(delay_ms);
            break;
        case 21: // 📐 角落扫描
            perform_corner_scan(delay_ms);
            break;
        case 22: // ➕ 十字形扫描
            perform_cross_pattern(delay_ms);
            break;
        case 23: // 💎 菱形扫描
            perform_diamond_scan(delay_ms);
            break;
        case 24: // 🫁 呼吸动作
            perform_breathing_action(delay_ms);
            break;
    }
    
    // 所有动作完成后返回初始位置
    ESP_LOGI(TAG, "🎭 动作完成，返回初始位置");
    current_servo_h = initial_h;
    current_servo_v = initial_v;
    current_servo_1 = initial_1;
    
    servo_set_angle(3, current_servo_h);
    servo_set_angle(2, current_servo_v);
    servo_set_angle(1, current_servo_1);
    
    vTaskDelay(pdMS_TO_TICKS(500)); // 等待返回初始位置
}

// 水平舵机摆动 (初始位置±20度，3-5次)
void perform_horizontal_swing(int delay_ms)
{
    int swing_count = 3 + (esp_random() % 3); // 3-5次
    int center = 110; // 水平舵机初始位置
    
    ESP_LOGI(TAG, "水平舵机摆动 %d 次", swing_count);
    
    for(int i = 0; i < swing_count; i++) {
        // 向左摆动 (center - 20) - 减少幅度
        current_servo_h = center - 20;
        servo_set_angle(3, current_servo_h);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        
        // 向右摆动 (center + 20) - 减少幅度
        current_servo_h = center + 20;
        servo_set_angle(3, current_servo_h);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// 垂直舵机摆动 (初始位置±20度，3-5次) - 减少幅度
void perform_vertical_swing(int delay_ms)
{
    int swing_count = 3 + (esp_random() % 3); // 3-5次
    int center = 120; // 垂直舵机初始位置
    
    ESP_LOGI(TAG, "垂直舵机摆动 %d 次", swing_count);
    
    for(int i = 0; i < swing_count; i++) {
        // 向上摆动 (center - 20) - 减少幅度
        current_servo_v = center - 20;
        servo_set_angle(2, current_servo_v);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        
        // 向下摆动 (center + 20) - 减少幅度
        current_servo_v = center + 20;
        servo_set_angle(2, current_servo_v);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// 1号舵机动作 (初始位置±15度，3-5次) - 减少幅度
void perform_servo1_action(int delay_ms)
{
    int swing_count = 3 + (esp_random() % 3); // 3-5次
    int center = 60; // 1号舵机初始位置
    
    ESP_LOGI(TAG, "1号舵机动作 %d 次", swing_count);
    
    for(int i = 0; i < swing_count; i++) {
        // 向一侧摆动 (center - 15) - 减少幅度
        current_servo_1 = center - 15;
        servo_set_angle(1, current_servo_1);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        
        // 向另一侧摆动 (center + 15) - 减少幅度
        current_servo_1 = center + 15;
        servo_set_angle(1, current_servo_1);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// 🌊 波浪动作 - 水平和垂直舵机协调运动，模拟波浪起伏 - 减少幅度
void perform_wave_action(int delay_ms)
{
    ESP_LOGI(TAG, "🌊 执行波浪动作");
    
    int h_center = 110, v_center = 115;
    int wave_cycles = 3; // 3个波浪周期
    
    for(int cycle = 0; cycle < wave_cycles; cycle++) {
        // 波浪上升阶段
        for(int step = 0; step <= 10; step++) {
            float angle = (step * 36.0) * M_PI / 180.0; // 0-360度转弧度
            current_servo_h = h_center + (int)(10 * sin(angle)); // 减少幅度：20->10
            current_servo_v = v_center + (int)(8 * cos(angle));  // 减少幅度：15->8
            
            servo_set_angle(3, current_servo_h);
            servo_set_angle(2, current_servo_v);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
        
        // 减速效果：逐渐增加延时
        delay_ms += 10;
    }
}

// 🔄 圆形扫描 - 三个舵机协调做圆形运动 - 减少幅度
void perform_circle_scan(int delay_ms)
{
    ESP_LOGI(TAG, "🔄 执行圆形扫描");
    
    int h_center = 110, v_center = 115, s1_center = 60;
    int steps = 16; // 16步完成一个圆
    
    for(int step = 0; step < steps; step++) {
        float angle = (step * 22.5) * M_PI / 180.0; // 每步22.5度
        
        current_servo_h = h_center + (int)(12 * cos(angle)); // 减少幅度：25->12
        current_servo_v = v_center + (int)(8 * sin(angle));  // 减少幅度：15->8
        current_servo_1 = s1_center + (int)(10 * sin(angle * 2)); // 减少幅度：20->10
        
        servo_set_angle(3, current_servo_h);
        servo_set_angle(2, current_servo_v);
        servo_set_angle(1, current_servo_1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        
        // 减速效果：后半段逐渐减速
        if(step > steps/2) {
            delay_ms += 5;
        }
    }
}

// ⚡ 震动模式 - 快速小幅度震动后逐渐减速 - 减少幅度
void perform_vibration_mode(int delay_ms)
{
    ESP_LOGI(TAG, "⚡ 执行震动模式");
    
    int h_center = 110, v_center = 115, s1_center = 60;
    int vibration_count = 20; // 20次震动
    int amplitude = 4; // 初始震动幅度 - 减少：8->4
    
    for(int i = 0; i < vibration_count; i++) {
        // 随机方向震动
        int h_offset = (esp_random() % (amplitude * 2)) - amplitude;
        int v_offset = (esp_random() % (amplitude * 2)) - amplitude;
        int s1_offset = (esp_random() % (amplitude * 2)) - amplitude;
        
        current_servo_h = h_center + h_offset;
        current_servo_v = v_center + v_offset;
        current_servo_1 = s1_center + s1_offset;
        
        servo_set_angle(3, current_servo_h);
        servo_set_angle(2, current_servo_v);
        servo_set_angle(1, current_servo_1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        
        // 减速效果：逐渐减小震动幅度和增加延时
        if(i > vibration_count/2) {
            amplitude = amplitude > 1 ? amplitude - 1 : 1; // 最小幅度改为1
            delay_ms += 3;
        }
    }
}

// 🎯 聚焦动作 - 从外围逐渐聚焦到中心点 - 减少幅度
void perform_focus_action(int delay_ms)
{
    ESP_LOGI(TAG, "🎯 执行聚焦动作");
    
    int h_center = 110, v_center = 115, s1_center = 60;
    int focus_steps = 8; // 8步聚焦
    
    for(int step = focus_steps; step >= 0; step--) {
        // 从外围逐渐收缩到中心
        int radius = step * 3; // 半径逐渐减小 - 减少：5->3
        
        // 四个方向的聚焦点
        int positions[][2] = {{radius, 0}, {0, radius}, {-radius, 0}, {0, -radius}};
        
        for(int pos = 0; pos < 4; pos++) {
            current_servo_h = h_center + positions[pos][0];
            current_servo_v = v_center + positions[pos][1];
            current_servo_1 = s1_center + (radius / 3); // 减少：radius/2 -> radius/3
            
            servo_set_angle(3, current_servo_h);
            servo_set_angle(2, current_servo_v);
            servo_set_angle(1, current_servo_1);
            
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
        
        // 减速效果：越接近中心越慢
        delay_ms += 15;
    }
}

// 🌟 星形轨迹 - 画五角星轨迹 - 减少幅度
void perform_star_trajectory(int delay_ms)
{
    ESP_LOGI(TAG, "🌟 执行星形轨迹");
    
    int h_center = 110, v_center = 115;
    int radius = 10; // 减少半径：20->10
    
    // 五角星的5个顶点角度 (度)
    float star_angles[] = {0, 144, 288, 72, 216}; // 每个点间隔144度
    
    for(int point = 0; point < 5; point++) {
        float angle = star_angles[point] * M_PI / 180.0;
        
        current_servo_h = h_center + (int)(radius * cos(angle));
        current_servo_v = v_center + (int)(radius * sin(angle));
        current_servo_1 = 60 + (int)(8 * sin(angle * 2)); // 减少幅度：15->8
        
        servo_set_angle(3, current_servo_h);
        servo_set_angle(2, current_servo_v);
        servo_set_angle(1, current_servo_1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        
        // 减速效果：后面的点移动更慢
        delay_ms += 20;
    }
    
    // 回到中心点
    current_servo_h = h_center;
    current_servo_v = v_center;
    servo_set_angle(3, current_servo_h);
    servo_set_angle(2, current_servo_v);
    vTaskDelay(pdMS_TO_TICKS(delay_ms + 50));
}

// 🔍 搜索模式 - 模拟雷达扫描搜索 - 减少幅度
void perform_search_mode(int delay_ms)
{
    ESP_LOGI(TAG, "🔍 执行搜索模式");
    
    int h_center = 110, v_center = 115;
    int search_rounds = 2; // 2轮搜索
    
    for(int round = 0; round < search_rounds; round++) {
        // 水平扫描 - 减少幅度：±30->±15
        for(int h = h_center - 15; h <= h_center + 15; h += 5) {
            current_servo_h = h;
            servo_set_angle(3, current_servo_h);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
        
        // 垂直扫描 - 减少幅度：±20->±10
        for(int v = v_center - 10; v <= v_center + 10; v += 4) {
            current_servo_v = v;
            servo_set_angle(2, current_servo_v);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
        
        // 1号舵机辅助搜索 - 减少幅度：20->10
        current_servo_1 = 60 + (round * 10);
        servo_set_angle(1, current_servo_1);
        
        // 减速效果：第二轮搜索更慢更仔细
        delay_ms += 25;
    }
}

// 💫 螺旋动作 - 从中心向外螺旋运动 - 减少幅度
void perform_spiral_action(int delay_ms)
{
    ESP_LOGI(TAG, "💫 执行螺旋动作");
    
    int h_center = 110, v_center = 115, s1_center = 60;
    int spiral_steps = 20; // 20步完成螺旋
    
    for(int step = 0; step < spiral_steps; step++) {
        float angle = (step * 36.0) * M_PI / 180.0; // 每步36度，多圈螺旋
        float radius = step * 0.8; // 半径逐渐增大 - 减少：1.5->0.8
        
        current_servo_h = h_center + (int)(radius * cos(angle));
        current_servo_v = v_center + (int)(radius * sin(angle));
        current_servo_1 = s1_center + (int)(5 * sin(angle * 3)); // 减少幅度：10->5
        
        servo_set_angle(3, current_servo_h);
        servo_set_angle(2, current_servo_v);
        servo_set_angle(1, current_servo_1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        
        // 减速效果：螺旋越大越慢
        if(step > spiral_steps/2) {
            delay_ms += 8;
        }
    }
}

// 新增10种随机动作函数实现

// 1. Z字形扫描动作
void perform_zigzag_action(int delay_ms)
{
    int h_center = 110, v_center = 115, s1_center = 60;
    int zigzag_steps = 8;
    
    for(int i = 0; i < zigzag_steps; i++) {
        // Z字形路径
        int h_offset = (i % 2 == 0) ? -15 : 15;
        int v_offset = -10 + (i * 3);
        
        current_servo_h = h_center + h_offset;
        current_servo_v = v_center + v_offset;
        current_servo_1 = s1_center + (i % 3 - 1) * 8;
        
        servo_set_angle(3, current_servo_h);
        servo_set_angle(2, current_servo_v);
        servo_set_angle(1, current_servo_1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// 2. 8字形轨迹动作
void perform_figure_eight(int delay_ms)
{
    int h_center = 110, v_center = 115, s1_center = 60;
    int steps = 16;
    
    for(int i = 0; i < steps; i++) {
        float t = (float)i / steps * 4 * M_PI;
        
        // 8字形参数方程
        current_servo_h = h_center + (int)(12 * sin(t));
        current_servo_v = v_center + (int)(8 * sin(t/2));
        current_servo_1 = s1_center + (int)(6 * cos(t));
        
        servo_set_angle(3, current_servo_h);
        servo_set_angle(2, current_servo_v);
        servo_set_angle(1, current_servo_1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// 3. 钟摆动作
void perform_pendulum_action(int delay_ms)
{
    int h_center = 110, v_center = 115, s1_center = 60;
    int pendulum_cycles = 6;
    
    for(int cycle = 0; cycle < pendulum_cycles; cycle++) {
        for(int i = 0; i <= 20; i++) {
            float angle = (float)i / 20 * M_PI - M_PI/2;
            
            current_servo_h = h_center + (int)(18 * sin(angle));
            current_servo_v = v_center + (int)(5 * cos(angle * 2));
            current_servo_1 = s1_center + (int)(8 * sin(angle));
            
            servo_set_angle(3, current_servo_h);
            servo_set_angle(2, current_servo_v);
            servo_set_angle(1, current_servo_1);
            
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
}

// 4. 随机游走动作
void perform_random_walk(int delay_ms)
{
    int h_pos = 110, v_pos = 115, s1_pos = 60;
    int walk_steps = 12;
    
    for(int i = 0; i < walk_steps; i++) {
        // 随机步长和方向
        int h_step = (esp_random() % 11) - 5; // -5到5
        int v_step = (esp_random() % 7) - 3;  // -3到3
        int s1_step = (esp_random() % 9) - 4; // -4到4
        
        h_pos = constrain(h_pos + h_step, 95, 125);
        v_pos = constrain(v_pos + v_step, 108, 122);
        s1_pos = constrain(s1_pos + s1_step, 52, 68);
        
        current_servo_h = h_pos;
        current_servo_v = v_pos;
        current_servo_1 = s1_pos;
        
        servo_set_angle(3, current_servo_h);
        servo_set_angle(2, current_servo_v);
        servo_set_angle(1, current_servo_1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms * 1.5));
    }
}

// 5. 脉冲动作
void perform_pulse_action(int delay_ms)
{
    int h_center = 110, v_center = 115, s1_center = 60;
    int pulse_count = 8;
    
    for(int pulse = 0; pulse < pulse_count; pulse++) {
        // 快速扩张
        for(int i = 0; i <= 5; i++) {
            float scale = (float)i / 5;
            
            current_servo_h = h_center + (int)(12 * scale);
            current_servo_v = v_center + (int)(8 * scale);
            current_servo_1 = s1_center + (int)(10 * scale);
            
            servo_set_angle(3, current_servo_h);
            servo_set_angle(2, current_servo_v);
            servo_set_angle(1, current_servo_1);
            
            vTaskDelay(pdMS_TO_TICKS(delay_ms / 2));
        }
        
        // 快速收缩
        for(int i = 5; i >= 0; i--) {
            float scale = (float)i / 5;
            
            current_servo_h = h_center + (int)(12 * scale);
            current_servo_v = v_center + (int)(8 * scale);
            current_servo_1 = s1_center + (int)(10 * scale);
            
            servo_set_angle(3, current_servo_h);
            servo_set_angle(2, current_servo_v);
            servo_set_angle(1, current_servo_1);
            
            vTaskDelay(pdMS_TO_TICKS(delay_ms / 2));
        }
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// 6. 平滑曲线动作
void perform_smooth_curve(int delay_ms)
{
    int h_center = 110, v_center = 115, s1_center = 60;
    int curve_steps = 20;
    
    for(int i = 0; i < curve_steps; i++) {
        float t = (float)i / curve_steps * 2 * M_PI;
        
        // 平滑的三角函数组合
        current_servo_h = h_center + (int)(15 * sin(t) * cos(t/2));
        current_servo_v = v_center + (int)(10 * cos(t) * sin(t/3));
        current_servo_1 = s1_center + (int)(8 * sin(t * 1.5));
        
        servo_set_angle(3, current_servo_h);
        servo_set_angle(2, current_servo_v);
        servo_set_angle(1, current_servo_1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// 7. 角落扫描动作
void perform_corner_scan(int delay_ms)
{
    int corners[][3] = {
        {95, 108, 52},   // 左上角
        {125, 108, 52},  // 右上角
        {125, 122, 68},  // 右下角
        {95, 122, 68},   // 左下角
        {110, 115, 60}   // 中心
    };
    
    for(int corner = 0; corner < 5; corner++) {
        current_servo_h = corners[corner][0];
        current_servo_v = corners[corner][1];
        current_servo_1 = corners[corner][2];
        
        servo_set_angle(3, current_servo_h);
        servo_set_angle(2, current_servo_v);
        servo_set_angle(1, current_servo_1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms * 2));
        
        // 在每个角落做小幅摆动
        for(int i = 0; i < 3; i++) {
            servo_set_angle(3, current_servo_h + (i % 2 ? 3 : -3));
            vTaskDelay(pdMS_TO_TICKS(delay_ms / 2));
        }
    }
}

// 8. 十字形扫描动作
void perform_cross_pattern(int delay_ms)
{
    int h_center = 110, v_center = 115, s1_center = 60;
    
    // 水平线扫描
    for(int h = 95; h <= 125; h += 3) {
        current_servo_h = h;
        current_servo_v = v_center;
        current_servo_1 = s1_center + (h - h_center) / 5;
        
        servo_set_angle(3, current_servo_h);
        servo_set_angle(2, current_servo_v);
        servo_set_angle(1, current_servo_1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    
    // 垂直线扫描
    for(int v = 108; v <= 122; v += 2) {
        current_servo_h = h_center;
        current_servo_v = v;
        current_servo_1 = s1_center + (v - v_center) / 3;
        
        servo_set_angle(3, current_servo_h);
        servo_set_angle(2, current_servo_v);
        servo_set_angle(1, current_servo_1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// 9. 菱形扫描动作
void perform_diamond_scan(int delay_ms)
{
    int h_center = 110, v_center = 115, s1_center = 60;
    int diamond_steps = 16;
    
    for(int i = 0; i < diamond_steps; i++) {
        float angle = (float)i / diamond_steps * 2 * M_PI;
        
        // 菱形轨迹（使用绝对值函数）
        float h_offset = 15 * (cos(angle) > 0 ? 1 : -1) * (1 - abs(sin(angle)));
        float v_offset = 10 * (sin(angle) > 0 ? 1 : -1) * (1 - abs(cos(angle)));
        
        current_servo_h = h_center + (int)h_offset;
        current_servo_v = v_center + (int)v_offset;
        current_servo_1 = s1_center + (int)(6 * sin(angle * 2));
        
        servo_set_angle(3, current_servo_h);
        servo_set_angle(2, current_servo_v);
        servo_set_angle(1, current_servo_1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// 10. 呼吸动作
void perform_breathing_action(int delay_ms)
{
    int h_center = 110, v_center = 115, s1_center = 60;
    int breath_cycles = 4;
    
    for(int cycle = 0; cycle < breath_cycles; cycle++) {
        // 吸气阶段 - 缓慢扩张
        for(int i = 0; i <= 15; i++) {
            float scale = (float)i / 15;
            float breath_factor = sin(scale * M_PI / 2); // 平滑的呼吸曲线
            
            current_servo_h = h_center + (int)(12 * breath_factor);
            current_servo_v = v_center + (int)(8 * breath_factor);
            current_servo_1 = s1_center + (int)(10 * breath_factor);
            
            servo_set_angle(3, current_servo_h);
            servo_set_angle(2, current_servo_v);
            servo_set_angle(1, current_servo_1);
            
            vTaskDelay(pdMS_TO_TICKS(delay_ms * 1.2));
        }
        
        // 呼气阶段 - 缓慢收缩
        for(int i = 15; i >= 0; i--) {
            float scale = (float)i / 15;
            float breath_factor = sin(scale * M_PI / 2);
            
            current_servo_h = h_center + (int)(12 * breath_factor);
            current_servo_v = v_center + (int)(8 * breath_factor);
            current_servo_1 = s1_center + (int)(10 * breath_factor);
            
            servo_set_angle(3, current_servo_h);
            servo_set_angle(2, current_servo_v);
            servo_set_angle(1, current_servo_1);
            
            vTaskDelay(pdMS_TO_TICKS(delay_ms * 1.5));
        }
        
        // 暂停
        vTaskDelay(pdMS_TO_TICKS(delay_ms * 2));
    }
}

// 舵机校准任务 - 暂停检测功能，专注于位置校准
void task_face_detect(void *arg)
{
    ESP_LOGI(TAG, "=== 人脸检测任务启动 ===");
    
    // 初始化人脸检测器
    HumanFaceDetectMSR01 detector(0.3F, 0.3F, 10, 0.3F);
    HumanFaceDetectMNP01 detector2(0.4F, 0.3F, 10);
    
    // 初始化移动检测器
    MotionDetect_t* motion_detector = motion_detect_init(320, 240);
    if (!motion_detector) {
        ESP_LOGE(TAG, "移动检测器初始化失败");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Motion detector initialized: 320x240");
    
    // PID控制器初始化
    PID_t pid_h = {0.8f, 0.0f, 0.1f, 0.0f, 0.0f}; // 水平PID
    PID_t pid_v = {0.8f, 0.0f, 0.1f, 0.0f, 0.0f}; // 垂直PID
    
    // 图像中心点 (320x240)
    const int img_center_x = 160;
    const int img_center_y = 120;
    
    // 随机动作相关变量
    int64_t last_action_time = esp_timer_get_time(); // 上次动作时间 (微秒)
    const int64_t IDLE_TIMEOUT = 30 * 1000 * 1000; // 30秒超时 (微秒) - 增加触发时间
    
    // 随机动作优先级控制
    bool random_action_in_progress = false; // 随机动作执行标志
    int64_t last_random_action_time = 0; // 上次随机动作时间
    const int64_t RANDOM_ACTION_INTERVAL = 120 * 1000 * 1000; // 120秒(2分钟)强制触发间隔

    // 1号舵机边界触发阈值
    const int SERVO_V_BOUNDARY_THRESHOLD = 15; // 距离边界15度时触发 (减小阈值)
    
    // 初始化舵机并进行测试动作
    servo_set_angle(2, 115);  // 垂直舵机初始角度115度
    servo_set_angle(3, 110);  // 水平舵机初始角度110度
    servo_set_angle(1, current_servo_1); // 1号舵机使用ID 1
    
    // 舵机测试动作 - 让用户确认哪个是1号哪个是2号
    ESP_LOGI(TAG, "开始舵机测试 - 1号舵机和2号舵机(垂直)测试");
    
    // 测试2号舵机(垂直舵机) - ID 2
    ESP_LOGI(TAG, "测试2号舵机(垂直) - 向上移动");
    servo_set_angle(2, 150);  // 向上
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "测试2号舵机(垂直) - 向下移动");
    servo_set_angle(2, 100);  // 向下
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "测试2号舵机(垂直) - 回到中位");
    servo_set_angle(2, 115);  // 回到初始位置
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 测试1号舵机 - ID 1
    ESP_LOGI(TAG, "测试1号舵机 - 移动到90度");
    servo_set_angle(1, 90);  // 移动到90度
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "测试1号舵机 - 移动到60度");
    servo_set_angle(1, 60);   // 移动到60度
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "测试1号舵机 - 回到60度位置");
    servo_set_angle(1, 60);  // 回到60度位置
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "舵机测试完成，开始正常运行");
    
    while (1)
    {
        // 优先检查随机动作触发条件 - 最高优先级
        int64_t current_time = esp_timer_get_time();
        
        // 检查强制触发条件（2分钟间隔）或无目标检测触发条件（30秒）
        bool should_trigger_random = false;
        if (current_time - last_random_action_time > RANDOM_ACTION_INTERVAL) {
            // 强制触发：2分钟间隔
            should_trigger_random = true;
            ESP_LOGI(TAG, "🎭 强制随机动作触发！2分钟间隔到达");
        } else if (current_time - last_action_time > IDLE_TIMEOUT) {
            // 无目标触发：30秒无检测
            should_trigger_random = true;
            ESP_LOGI(TAG, "🎭 随机动作触发！30秒无目标检测");
        }
        
        if (should_trigger_random && !random_action_in_progress) {
            random_action_in_progress = true;
            ESP_LOGI(TAG, "🎭 开始执行随机动作序列 - 最高优先级");
            
            // 触发复杂随机动作组合
            perform_random_action_sequence();
            
            // 更新时间记录
            last_action_time = current_time;
            last_random_action_time = current_time;
            random_action_in_progress = false;
            
            ESP_LOGI(TAG, "🎭 随机动作序列执行完成，恢复正常检测模式");
            continue; // 跳过本次检测循环，确保随机动作优先级
        }

        // 获取摄像头帧
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb)
        {
            ESP_LOGW(TAG, "获取摄像头帧失败");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        // 人脸检测
        std::list<dl::detect::result_t> candidates = detector.infer(
            (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3});
        std::list<dl::detect::result_t> face_results = detector2.infer(
            (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3}, candidates);
        
        // 移动检测
        motion_detect_process(motion_detector, (uint16_t *)fb->buf);
        
        // 目标仲裁：优先人脸，其次移动物体
        TargetInfo target = arbitrate_target(face_results, motion_detector, (uint16_t *)fb->buf);
        
        // 记录目标信息
        log_target(target);
        
        if (target.detected)
        {
            // 更新上次动作时间
            last_action_time = esp_timer_get_time();
            
            // 计算目标偏移量
            int error_x = target.cx - img_center_x;
            int error_y = target.cy - img_center_y;
            
            // PID控制计算
            float pid_output_h = pid_compute(&pid_h, 0, error_x);
            float pid_output_v = pid_compute(&pid_v, 0, error_y);
            
            // 更新舵机角度 (修正移动方向，增大动作幅度)
            current_servo_h += (int)(pid_output_h * 0.3f); // 水平方向调整 (增大幅度)
            current_servo_v -= (int)(pid_output_v * 0.3f); // 垂直方向调整 (增大幅度)
            
            // 限制舵机角度范围
            current_servo_h = std::max(SERVO_H_MIN, std::min(SERVO_H_MAX, current_servo_h));
            current_servo_v = std::max(SERVO_V_MIN, std::min(SERVO_V_MAX, current_servo_v));
            
            // 检查垂直舵机是否接近边界，触发1号舵机相关动作
            bool near_v_boundary = false;
            int servo1_target_angle = current_servo_1;
            
            if (current_servo_v <= SERVO_V_MIN + SERVO_V_BOUNDARY_THRESHOLD) {
                // 接近下边界，1号舵机向一个方向移动
                near_v_boundary = true;
                servo1_target_angle = 50; // 调整为50度（60度以内）
                ESP_LOGI(TAG, "垂直舵机接近下边界 (%d), 1号舵机调整到 %d", current_servo_v, servo1_target_angle);
            } else if (current_servo_v >= SERVO_V_MAX - SERVO_V_BOUNDARY_THRESHOLD) {
                // 接近上边界，1号舵机向另一个方向移动
                near_v_boundary = true;
                servo1_target_angle = 50; // 调整为50度（60度以内）
                ESP_LOGI(TAG, "垂直舵机接近上边界 (%d), 1号舵机调整到 %d", current_servo_v, servo1_target_angle);
            } else {
                // 不在边界附近，1号舵机缓慢回到60度位置
                servo1_target_angle = 60;
            }
            
            // 平滑调整1号舵机角度 (避免突然跳跃)
            if (current_servo_1 != servo1_target_angle) {
                int diff = servo1_target_angle - current_servo_1;
                int step = (diff > 0) ? std::min(3, diff) : std::max(-3, diff); // 每次最多移动3度
                current_servo_1 += step;
                
                // 限制1号舵机角度范围
                current_servo_1 = std::max(SERVO_1_MIN, std::min(SERVO_1_MAX, current_servo_1));
                
                servo_set_angle(1, current_servo_1); // 1号舵机
            }
            
            // 设置舵机角度
            servo_set_angle(3, current_servo_h); // 水平舵机
            servo_set_angle(2, current_servo_v); // 垂直舵机
            
            // 优化日志输出 - 只在角度变化或时间间隔足够时输出
            int64_t current_time = esp_timer_get_time();
            bool should_log = (current_time - last_log_time > LOG_INTERVAL_US) ||
                             (current_servo_h != last_logged_servo_h) ||
                             (current_servo_v != last_logged_servo_v) ||
                             (current_servo_1 != last_logged_servo_1);
            
            if (should_log) {
                ESP_LOGI(TAG, "Target center: (%d,%d) -> servo H: %d, V: %d, Servo1: %d", 
                         target.cx, target.cy, current_servo_h, current_servo_v, current_servo_1);
                
                // 更新记录的值
                last_logged_servo_h = current_servo_h;
                last_logged_servo_v = current_servo_v;
                last_logged_servo_1 = current_servo_1;
                last_log_time = current_time;
            }
        }
        
        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(300)); // 降低处理频率到约3FPS
    }
    
    // 清理资源
    motion_detect_free(motion_detector);
}

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 10000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565, // 注意：HTTP 发送用 JPEG 最合适
    .frame_size = FRAMESIZE_QVGA,     // QVGA 320x240
    .jpeg_quality = 12,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    .sccb_i2c_port = 0,
};

static esp_err_t init_camera(void)
{
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }
    return ESP_OK;
}

// ---------------- app_main ----------------
extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    // 初始化摄像头
    if (ESP_OK != init_camera())
    {
        ESP_LOGE(TAG, "Camera init failed");
        return;
    }

    // 初始化舵机
    servo_init();
    
    // 设置舵机初始位置并打印PWM状态
    ESP_LOGI(TAG, "Setting initial servo positions...");
    servo_set_angle(2, 115);  // 垂直舵机初始角度115度
    servo_set_angle(3, 110);  // 水平舵机初始角度110度
    
    // 等待舵机到位
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 打印所有舵机的PWM状态
    servo_print_all_pwm();

    // 启动人脸检测任务
    xTaskCreate(task_face_detect, "face_detect", 4 * 1024, NULL, 5, NULL);
}
