#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "servo.h"
#include "camera_color_detection.h"
#include "wifi_server.h"
#include "optical_flow.h"
#include "esp_log.h"

static const char *TAG = "main";
static httpd_handle_t web_server = NULL;
static optical_flow_tracker_t optical_flow_tracker;
static bool use_optical_flow = false;  // 光流跟踪开关



/**
 * @brief 色块跟踪函数
 * @param blob 检测到的色块信息
 */
void track_color_blob(const color_blob_t *blob)
{
    if (!blob->found) {
        return;
    }
    
    // 图像中心坐标 (QVGA: 320x240)
    int image_center_x = 160;
    int image_center_y = 120;
    
    // 计算色块相对于图像中心的偏移
    int offset_x = blob->x_center - image_center_x;
    int offset_y = blob->y_center - image_center_y;
    
    // 根据偏移量控制舵机 (简单比例控制)
    // 水平方向：SERVO_1 (左右)
    int servo1_angle = 90 + (offset_x * 90 / image_center_x);
    servo1_angle = servo1_angle < 0 ? 0 : (servo1_angle > 180 ? 180 : servo1_angle);
    
    // 垂直方向：SERVO_2 (上下)
    int servo2_angle = 90 - (offset_y * 90 / image_center_y);
    servo2_angle = servo2_angle < 0 ? 0 : (servo2_angle > 180 ? 180 : servo2_angle);
    
    // 控制舵机跟踪色块
    servo_set_angle(SERVO_1_CHANNEL, servo1_angle);
    servo_set_angle(SERVO_2_CHANNEL, servo2_angle);
    
    printf("跟踪色块: 偏移(%d,%d) -> 舵机角度(%d,%d)\n", 
           offset_x, offset_y, servo1_angle, servo2_angle);
}

/**
 * @brief 基于光流的运动跟踪函数
 * @param motion 光流估计的运动信息
 */
void track_optical_flow_motion(const motion_estimate_t *motion)
{
    if (!motion->motion_detected || motion->confidence < 0.3f) {
        return;
    }
    
    // 获取当前舵机角度
    static int current_servo1_angle = 90;
    static int current_servo2_angle = 90;
    
    // 根据光流运动调整舵机角度
    // 光流运动方向与舵机转动方向相反（补偿运动）
    float motion_scale = 0.5f;  // 运动缩放因子
    
    int angle_adjust_x = (int)(-motion->motion_x * motion_scale);
    int angle_adjust_y = (int)(-motion->motion_y * motion_scale);
    
    // 更新舵机角度
    current_servo1_angle += angle_adjust_x;
    current_servo2_angle += angle_adjust_y;
    
    // 限制角度范围
    current_servo1_angle = current_servo1_angle < 0 ? 0 : 
                          (current_servo1_angle > 180 ? 180 : current_servo1_angle);
    current_servo2_angle = current_servo2_angle < 0 ? 0 : 
                          (current_servo2_angle > 180 ? 180 : current_servo2_angle);
    
    // 控制舵机
    servo_set_angle(SERVO_1_CHANNEL, current_servo1_angle);
    servo_set_angle(SERVO_2_CHANNEL, current_servo2_angle);
    
    printf("光流跟踪: 运动(%.2f,%.2f) -> 舵机角度(%d,%d), 置信度=%.2f\n", 
           motion->motion_x, motion->motion_y, 
           current_servo1_angle, current_servo2_angle, motion->confidence);
}

/**
 * @brief 摄像头运动跟踪与替换任务（肤色检测 + 光流算法 + 运动替换）
 */
void camera_tracking_task(void *pvParameters)
{
    printf("摄像头运动跟踪与替换任务启动\n");
    
    // 初始化光流跟踪器
    if (optical_flow_init(&optical_flow_tracker, 320, 240) != ESP_OK) {
        ESP_LOGE(TAG, "光流跟踪器初始化失败");
        use_optical_flow = false;
    } else {
        ESP_LOGI(TAG, "光流跟踪器初始化成功");
        use_optical_flow = true;
    }
    
    int frame_count = 0;
    bool color_blob_found = false;
    
    while (1) {
        // 捕获图像
        camera_fb_t *fb = camera_capture();
        if (!fb) {
            printf("图像捕获失败\n");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        frame_count++;
        color_blob_found = false;
        
        // 每隔几帧进行手指检测（降低计算负载）
        if (frame_count % 3 == 0) {
            // 检测手指（肤色）并跟踪
            color_blob_t finger_blob;
            if (detect_color_blob(fb, &SKIN_THRESHOLD, &finger_blob) == ESP_OK) {
                // 添加面积和形状过滤，确保检测到的是手指而不是其他肤色区域
                if (finger_blob.found && 
                    finger_blob.area > 50 && finger_blob.area < 5000 &&  // 面积范围过滤
                    finger_blob.width > 10 && finger_blob.height > 10) {   // 尺寸过滤
                    
                    printf("检测到手指，开始跟踪 - 位置:(%d,%d), 尺寸:%dx%d, 面积:%d\n", 
                           finger_blob.x_center, finger_blob.y_center, 
                           finger_blob.width, finger_blob.height, finger_blob.area);
                    
                    track_color_blob(&finger_blob);
                    color_blob_found = true;
                    
                    // 在图像上绘制手指检测框（用于调试）
                    detect_and_draw_color_blob(fb, &SKIN_THRESHOLD, &finger_blob, 0x07E0); // 绿色框
                }
            }
        }
        
        // 如果启用光流，更新光流跟踪器（无论是否检测到色块）
        if (use_optical_flow) {
            // 更新光流跟踪器
            if (optical_flow_update(&optical_flow_tracker, fb) == ESP_OK) {
                // 估计运动
                motion_estimate_t motion;
                if (estimate_motion(&optical_flow_tracker, &motion) == ESP_OK) {
                    if (motion.motion_detected) {
                        // 检测运动区域并替换为白色
                        float motion_threshold = 2.0f; // 运动阈值
                        detect_and_replace_motion_with_white(fb, &optical_flow_tracker, motion_threshold);
                        
                        // 如果没有检测到色块，使用光流跟踪舵机
                        if (!color_blob_found) {
                            track_optical_flow_motion(&motion);
                        }
                        
                        // 在图像上绘制光流（用于调试）
                        draw_optical_flow(fb, &optical_flow_tracker, 0x07E0); // 绿色
                    }
                }
            }
        }
        
        // 释放图像缓冲区
        camera_fb_return(fb);
        
        // 控制帧率，光流算法需要更高的帧率
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // 让其他任务有机会运行
        taskYIELD();
    }
    
    // 清理光流跟踪器
    if (use_optical_flow) {
        optical_flow_deinit(&optical_flow_tracker);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32摄像头色块跟踪系统启动");
    
    // 初始化舵机
    if (servo_init() != ESP_OK) {
        ESP_LOGE(TAG, "舵机初始化失败");
        return;
    }
    ESP_LOGI(TAG, "舵机初始化成功");
    
    // 初始化摄像头
    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "摄像头初始化失败");
        return;
    }
    ESP_LOGI(TAG, "摄像头初始化成功");
    
    // 初始化WiFi热点
    if (wifi_init_softap() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi热点初始化失败");
        return;
    }
    ESP_LOGI(TAG, "WiFi热点初始化成功");
    
    // 启动HTTP服务器
    web_server = start_webserver();
    if (web_server == NULL) {
        ESP_LOGE(TAG, "HTTP服务器启动失败");
        return;
    }
    ESP_LOGI(TAG, "HTTP服务器启动成功");
    
    // 延时等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 舵机回到中位
    servo_set_all_angle(90);
    printf("舵机回到中位\n");
    
    // 单独控制另外两路舵机到90度
    servo_set_angle(SERVO_3_CHANNEL, 90);
    servo_set_angle(SERVO_4_CHANNEL, 90);
    ESP_LOGI(TAG, "SERVO_3和SERVO_4已设置为90度");
    
    // 显示网络信息
    ESP_LOGI(TAG, "=== 网络服务信息 ===");
    ESP_LOGI(TAG, "WiFi热点名称: %s", WIFI_SSID);
    ESP_LOGI(TAG, "WiFi密码: %s", WIFI_PASS);
    ESP_LOGI(TAG, "访问地址: http://192.168.4.1");
    ESP_LOGI(TAG, "===================");
    
    // 根据模式选择运行方式
    #ifdef TEST_MODE
        ESP_LOGI(TAG, "进入测试模式");
        servo_test();
        // 测试光流算法
        optical_flow_test();
    #else
        ESP_LOGI(TAG, "进入运动跟踪与替换模式（手指跟踪 + 光流算法 + 运动替换）");
        // 创建摄像头手指跟踪任务
        xTaskCreate(camera_tracking_task, "finger_tracking", 8192, NULL, 5, NULL);
    #endif
    
    // 主循环
    while (1) {
        // 检查服务器状态
        if (web_server == NULL) {
            ESP_LOGW(TAG, "HTTP服务器已停止，尝试重启...");
            web_server = start_webserver();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
