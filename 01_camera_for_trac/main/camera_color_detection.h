#ifndef CAMERA_COLOR_DETECTION_H
#define CAMERA_COLOR_DETECTION_H

#include "esp_camera.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include "optical_flow.h"

// 摄像头引脚定义 (ESP32-CAM)
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    17
#define CAM_PIN_SIOD    20
#define CAM_PIN_SIOC    19
#define CAM_PIN_D7      42
#define CAM_PIN_D6      41
#define CAM_PIN_D5      40
#define CAM_PIN_D4      39
#define CAM_PIN_D3      38
#define CAM_PIN_D2      13
#define CAM_PIN_D1      12
#define CAM_PIN_D0      11
#define CAM_PIN_VSYNC   8
#define CAM_PIN_HREF    18
#define CAM_PIN_PCLK    16

// 颜色阈值结构体
typedef struct {
    uint8_t r_min, r_max;
    uint8_t g_min, g_max;
    uint8_t b_min, b_max;
} color_threshold_t;

// 色块信息结构体
typedef struct {
    int x_center;       // 色块中心X坐标
    int y_center;       // 色块中心Y坐标
    int width;          // 色块宽度
    int height;         // 色块高度
    int area;           // 色块面积
    bool found;         // 是否找到色块
} color_blob_t;

// 预定义颜色阈值
extern const color_threshold_t RED_THRESHOLD;
extern const color_threshold_t GREEN_THRESHOLD;
extern const color_threshold_t BLUE_THRESHOLD;
extern const color_threshold_t YELLOW_THRESHOLD;
extern const color_threshold_t SKIN_THRESHOLD;  // 肤色阈值

/**
 * @brief 初始化摄像头
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t camera_init(void);

/**
 * @brief 反初始化摄像头
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t camera_deinit(void);

/**
 * @brief 捕获一帧图像
 * @return 图像缓冲区指针，NULL表示失败
 */
camera_fb_t* camera_capture(void);

/**
 * @brief 释放图像缓冲区
 * @param fb 图像缓冲区指针
 */
void camera_fb_return(camera_fb_t *fb);

/**
 * @brief RGB565转RGB888
 * @param rgb565 RGB565像素值
 * @param r 红色分量输出
 * @param g 绿色分量输出
 * @param b 蓝色分量输出
 */
void rgb565_to_rgb888(uint16_t rgb565, uint8_t *r, uint8_t *g, uint8_t *b);

/**
 * @brief 检查像素是否在颜色阈值范围内
 * @param r 红色分量
 * @param g 绿色分量
 * @param b 蓝色分量
 * @param threshold 颜色阈值
 * @return true 在范围内，false 不在范围内
 */
bool is_color_in_threshold(uint8_t r, uint8_t g, uint8_t b, const color_threshold_t *threshold);

/**
 * @brief 在图像中检测指定颜色的色块
 * @param fb 图像缓冲区
 * @param threshold 颜色阈值
 * @param blob 检测到的色块信息输出
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t detect_color_blob(camera_fb_t *fb, const color_threshold_t *threshold, color_blob_t *blob);

/**
 * @brief 打印色块信息
 * @param blob 色块信息
 * @param color_name 颜色名称
 */
void print_blob_info(const color_blob_t *blob, const char *color_name);

/**
 * @brief 在RGB565图像上绘制矩形框
 * @param fb 图像缓冲区
 * @param x 矩形左上角X坐标
 * @param y 矩形左上角Y坐标
 * @param width 矩形宽度
 * @param height 矩形高度
 * @param color RGB565颜色值
 * @param thickness 线条粗细
 */
void draw_rectangle_rgb565(camera_fb_t *fb, int x, int y, int width, int height, uint16_t color, int thickness);

/**
 * @brief 在图像中检测指定颜色的色块并绘制轮廓
 * @param fb 图像缓冲区
 * @param threshold 颜色阈值
 * @param blob 检测到的色块信息输出
 * @param draw_color 绘制矩形框的颜色(RGB565)
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t detect_and_draw_color_blob(camera_fb_t *fb, const color_threshold_t *threshold, color_blob_t *blob, uint16_t draw_color);

/**
 * @brief 摄像头颜色检测测试函数
 */
void camera_color_detection_test(void);

/**
 * @brief 检测运动区域并替换为白色
 * @param fb 图像缓冲区
 * @param tracker 光流跟踪器
 * @param motion_threshold 运动阈值
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t detect_and_replace_motion_with_white(camera_fb_t *fb, const optical_flow_tracker_t *tracker, float motion_threshold);

/**
 * @brief 基于光流向量创建运动掩码
 * @param tracker 光流跟踪器
 * @param motion_mask 运动掩码输出（需要预分配内存）
 * @param width 图像宽度
 * @param height 图像高度
 * @param motion_threshold 运动阈值
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t create_motion_mask(const optical_flow_tracker_t *tracker, uint8_t *motion_mask, int width, int height, float motion_threshold);

/**
 * @brief 将运动区域的像素替换为白色
 * @param fb 图像缓冲区
 * @param motion_mask 运动掩码
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t replace_motion_pixels_with_white(camera_fb_t *fb, const uint8_t *motion_mask);

#endif // CAMERA_COLOR_DETECTION_H