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

// 函数声明
void perform_horizontal_swing(int delay_ms);
void perform_vertical_swing(int delay_ms);
void perform_servo1_action(int delay_ms);

// 随机动作系统 - 3个基本动作的7种组合
void perform_random_action_sequence()
{
    // 随机选择动作组合 (1-7)
    int action_combo = 1 + (esp_random() % 7);
    
    // 随机选择速度 (0=慢速, 1=快速)
    bool fast_speed = (esp_random() % 2) == 1;
    int delay_ms = fast_speed ? 50 : 100; // 快速50ms, 慢速100ms
    
    ESP_LOGI(TAG, "执行随机动作组合 %d (%s速度)", action_combo, fast_speed ? "快" : "慢");
    
    // 记录初始位置
    int initial_h = 110;  // 水平舵机初始位置
    int initial_v = 120;  // 垂直舵机初始位置  
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
    }
    
    // 所有动作完成后返回初始位置
    ESP_LOGI(TAG, "动作完成，返回初始位置");
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
        // 向左摆动 (center - 40)
        current_servo_h = center - 40;
        servo_set_angle(3, current_servo_h);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        
        // 向右摆动 (center + 40)
        current_servo_h = center + 40;
        servo_set_angle(3, current_servo_h);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// 垂直舵机摆动 (初始位置±40度，3-5次)
void perform_vertical_swing(int delay_ms)
{
    int swing_count = 3 + (esp_random() % 3); // 3-5次
    int center = 120; // 垂直舵机初始位置
    
    ESP_LOGI(TAG, "垂直舵机摆动 %d 次", swing_count);
    
    for(int i = 0; i < swing_count; i++) {
        // 向上摆动 (center - 40)
        current_servo_v = center - 40;
        servo_set_angle(2, current_servo_v);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        
        // 向下摆动 (center + 40)
        current_servo_v = center + 40;
        servo_set_angle(2, current_servo_v);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// 1号舵机到60度位置
void perform_servo1_action(int delay_ms)
{
    ESP_LOGI(TAG, "1号舵机移动到60度");
    
    current_servo_1 = 60;
    servo_set_angle(1, current_servo_1);
    vTaskDelay(pdMS_TO_TICKS(delay_ms * 3)); // 在60度位置停留更长时间
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
    
    // 当前舵机角度 (使用新的初始角度) - 已移动到全局变量
    // int current_servo_h = 110; // 水平初始角度110度
    // int current_servo_v = 120; // 垂直初始角度120度
    // int current_servo_1 = 120; // 1号舵机初始角度120度
    
    // 随机动作相关变量
    int64_t last_action_time = esp_timer_get_time(); // 上次动作时间 (微秒)
    const int64_t IDLE_TIMEOUT = 20 * 1000 * 1000; // 20秒超时 (微秒)
    
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
        else
        {
            // 检查是否超过20秒无动作
            int64_t current_time = esp_timer_get_time();
            if (current_time - last_action_time > IDLE_TIMEOUT)
            {
                // 添加随机动作触发的特殊提醒日志
                ESP_LOGI(TAG, "🎭 随机动作触发！20秒无目标检测，开始执行随机动作序列");
                
                // 触发复杂随机动作组合
                perform_random_action_sequence();
                
                // 更新上次动作时间
                last_action_time = current_time;
                
                ESP_LOGI(TAG, "🎭 随机动作序列执行完成，恢复正常检测模式");
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

// ---------------- Wi-Fi 配置 ----------------
// #define WIFI_SSID "myssid"     // 修改为你的 WiFi 名
// #define WIFI_PASS "mypassword" // 修改为你的 WiFi 密码
// #define MAX_RETRY 5

// static EventGroupHandle_t s_wifi_event_group;
// static const int WIFI_CONNECTED_BIT = BIT0;
// static const int WIFI_FAIL_BIT = BIT1;
// static int s_retry_num = 0;

// static const char *TAG = "cam_http";

// // ---------------- 摄像头引脚定义 ----------------
// #define CAM_PIN_PWDN -1
// #define CAM_PIN_RESET -1
// #define CAM_PIN_XCLK 17
// #define CAM_PIN_SIOD 20
// #define CAM_PIN_SIOC 19
// #define CAM_PIN_D7 42
// #define CAM_PIN_D6 41
// #define CAM_PIN_D5 40
// #define CAM_PIN_D4 39
// #define CAM_PIN_D3 38
// #define CAM_PIN_D2 13
// #define CAM_PIN_D1 12
// #define CAM_PIN_D0 11
// #define CAM_PIN_VSYNC 8
// #define CAM_PIN_HREF 18
// #define CAM_PIN_PCLK 16

// static camera_config_t camera_config = {
//     .pin_pwdn = CAM_PIN_PWDN,
//     .pin_reset = CAM_PIN_RESET,
//     .pin_xclk = CAM_PIN_XCLK,
//     .pin_sccb_sda = CAM_PIN_SIOD,
//     .pin_sccb_scl = CAM_PIN_SIOC,

//     .pin_d7 = CAM_PIN_D7,
//     .pin_d6 = CAM_PIN_D6,
//     .pin_d5 = CAM_PIN_D5,
//     .pin_d4 = CAM_PIN_D4,
//     .pin_d3 = CAM_PIN_D3,
//     .pin_d2 = CAM_PIN_D2,
//     .pin_d1 = CAM_PIN_D1,
//     .pin_d0 = CAM_PIN_D0,
//     .pin_vsync = CAM_PIN_VSYNC,
//     .pin_href = CAM_PIN_HREF,
//     .pin_pclk = CAM_PIN_PCLK,

//     .xclk_freq_hz = 10000000,
//     .ledc_timer = LEDC_TIMER_0,
//     .ledc_channel = LEDC_CHANNEL_0,

//     .pixel_format = PIXFORMAT_RGB565, // 注意：HTTP 发送用 JPEG 最合适
//     .frame_size = FRAMESIZE_QVGA,     // QVGA 320x240
//     .jpeg_quality = 12,
//     .fb_count = 1,
//     .fb_location = CAMERA_FB_IN_PSRAM,
//     .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
//     .sccb_i2c_port = 0,
// };

// ---------------- Wi-Fi 事件回调 ----------------
// static void event_handler(void *arg, esp_event_base_t event_base,
//                           int32_t event_id, void *event_data)
// {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
//     {
//         esp_wifi_connect();
//     }
//     else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
//     {
//         if (s_retry_num < MAX_RETRY)
//         {
//             esp_wifi_connect();
//             s_retry_num++;
//             ESP_LOGI(TAG, "retry to connect to the AP");
//         }
//         else
//         {
//             xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
//         }
//         ESP_LOGI(TAG, "connect to the AP fail");
//     }
//     else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
//     {
//         ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
//         ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
//         s_retry_num = 0;
//         xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
//     }
// }

// ---------------- Wi-Fi 初始化 ----------------
// void wifi_init_sta(void)
// {
//     s_wifi_event_group = xEventGroupCreate();

//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     esp_event_handler_instance_t instance_any_id;
//     esp_event_handler_instance_t instance_got_ip;
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
//                                                         ESP_EVENT_ANY_ID,
//                                                         &event_handler,
//                                                         NULL,
//                                                         &instance_any_id));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
//                                                         IP_EVENT_STA_GOT_IP,
//                                                         &event_handler,
//                                                         NULL,
//                                                         &instance_got_ip));

//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid = WIFI_SSID,
//             .password = WIFI_PASS,
//         },
//     };
//     wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());

//     ESP_LOGI(TAG, "wifi_init_sta finished.");

//     EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
//                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
//                                            pdFALSE,
//                                            pdFALSE,
//                                            portMAX_DELAY);

//     if (bits & WIFI_CONNECTED_BIT)
//     {
//         ESP_LOGI(TAG, "connected to ap SSID:%s", WIFI_SSID);
//     }
//     else if (bits & WIFI_FAIL_BIT)
//     {
//         ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
//     }
//     else
//     {
//         ESP_LOGE(TAG, "UNEXPECTED EVENT");
//     }
// }

// ---------------- 摄像头初始化 ----------------
// static esp_err_t init_camera(void)
// {
//     esp_err_t err = esp_camera_init(&camera_config);
//     if (err != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Camera Init Failed");
//         return err;
//     }
//     return ESP_OK;
// }

// ---------------- HTTP JPG Handler ----------------
// esp_err_t jpg_httpd_handler(httpd_req_t *req){
//     camera_fb_t * fb = NULL;
//     esp_err_t res = ESP_OK;

//     fb = esp_camera_fb_get();
//     if (!fb) {
//         ESP_LOGE(TAG, "Camera capture failed");
//         httpd_resp_send_500(req);
//         return ESP_FAIL;
//     }

//     httpd_resp_set_type(req, "image/jpeg");
//     httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

//     res = httpd_resp_send(req, (const char *)fb->buf, fb->len);

//     esp_camera_fb_return(fb);
//     return res;
// }
// ---------------- HTTP JPG Handler with Face Detection ----------------
// esp_err_t jpg_httpd_handler(httpd_req_t *req){
//     camera_fb_t *fb = nullptr;
//     esp_err_t res = ESP_OK;

//     // 初始化人脸检测器
//     HumanFaceDetectMSR01 detector(0.3F, 0.3F, 10, 0.3F);
//     HumanFaceDetectMNP01 detector2(0.4F, 0.3F, 10);

//     while (true) {
//         fb = esp_camera_fb_get();
//         if (!fb) {
//             ESP_LOGE(TAG, "Camera capture failed");
//             httpd_resp_send_500(req);
//             return ESP_FAIL;
//         }

//         // 检测人脸
//         std::list<dl::detect::result_t> &candidates = detector.infer(
//             (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3});
//         std::list<dl::detect::result_t> &results = detector2.infer(
//             (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3}, candidates);

//         if (!results.empty()) {
//             // 画检测框
//             draw_detection_result((uint16_t *)fb->buf, fb->height, fb->width, results);
//             print_detection_result(results);

//             // 转 JPEG
//             size_t jpg_len = 0;
//             uint8_t *jpg_buf = NULL;
//             if (!frame2jpg(fb, 80, &jpg_buf, &jpg_len)) {
//                 ESP_LOGE(TAG, "JPEG conversion failed");
//                 esp_camera_fb_return(fb);
//                 httpd_resp_send_500(req);
//                 return ESP_FAIL;
//             }

//             // HTTP 返回
//             httpd_resp_set_type(req, "image/jpeg");
//             httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
//             res = httpd_resp_send(req, (const char *)jpg_buf, jpg_len);

//             free(jpg_buf);
//             esp_camera_fb_return(fb);
//             return res;  // 检测到人脸就退出循环，返回结果
//         }

//         // 没检测到人脸，释放帧，继续循环
//         esp_camera_fb_return(fb);
//         vTaskDelay(pdMS_TO_TICKS(50)); // 50ms 延时，降低 CPU 占用
//     }
// }

// ---------------- 启动 HTTP 服务 ----------------
// static httpd_handle_t start_webserver(void)
// {
//     httpd_config_t config = HTTPD_DEFAULT_CONFIG();
//     httpd_handle_t server = NULL;

//     if (httpd_start(&server, &config) == ESP_OK)
//     {
//         httpd_uri_t uri_jpg = {
//             .uri = "/jpg",
//             .method = HTTP_GET,
//             .handler = jpg_httpd_handler,
//             .user_ctx = NULL};
//         httpd_register_uri_handler(server, &uri_jpg);
//     }
//     return server;
// }

// ---------------- 主程序 ----------------
// extern "C" void app_main(void)
// {
//     // ESP_ERROR_CHECK(nvs_flash_init());

//     // ESP_LOGI(TAG, "Starting Wi-Fi...");
//     // wifi_init_sta();

//     ESP_LOGI(TAG, "Init camera...");
//     if (ESP_OK != init_camera())
//     {
//         return;
//     }

//     ESP_LOGI(TAG, "Starting web server...");
//     // start_webserver();

//     // 主线程不用做别的，HTTP 请求来了就会自动调用 handler
//     while (1)
//     {
//         vTaskDelay(1000 / portTICK_PERIOD_MS);
//     }
// }
