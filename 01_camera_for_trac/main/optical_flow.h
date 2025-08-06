#ifndef OPTICAL_FLOW_H
#define OPTICAL_FLOW_H

#include "esp_camera.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// 光流算法配置参数
#define MAX_FEATURES 20          // 最大特征点数量
#define WINDOW_SIZE 5            // 光流窗口大小
#define MAX_ITERATIONS 10        // 最大迭代次数
#define CONVERGENCE_THRESHOLD 0.1f  // 收敛阈值
#define MIN_EIGENVALUE 0.01f     // 最小特征值阈值
#define PYRAMID_LEVELS 2         // 金字塔层数

// 特征点结构体
typedef struct {
    float x, y;              // 特征点坐标
    bool valid;              // 特征点是否有效
    float response;          // 角点响应值
} feature_point_t;

// 光流向量结构体
typedef struct {
    float dx, dy;            // 光流向量
    bool tracked;            // 是否成功跟踪
    float error;             // 跟踪误差
} optical_flow_t;

// 光流跟踪器结构体
typedef struct {
    feature_point_t features[MAX_FEATURES];  // 特征点数组
    optical_flow_t flows[MAX_FEATURES];      // 光流向量数组
    int feature_count;                       // 当前特征点数量
    uint8_t *prev_frame;                     // 前一帧图像
    uint8_t *curr_frame;                     // 当前帧图像
    int width, height;                       // 图像尺寸
    bool initialized;                        // 是否已初始化
} optical_flow_tracker_t;

// 运动估计结果结构体
typedef struct {
    float motion_x, motion_y;    // 平均运动向量
    float confidence;            // 置信度
    int tracked_points;          // 成功跟踪的点数
    bool motion_detected;        // 是否检测到运动
} motion_estimate_t;

/**
 * @brief 初始化光流跟踪器
 * @param tracker 光流跟踪器指针
 * @param width 图像宽度
 * @param height 图像高度
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t optical_flow_init(optical_flow_tracker_t *tracker, int width, int height);

/**
 * @brief 释放光流跟踪器资源
 * @param tracker 光流跟踪器指针
 */
void optical_flow_deinit(optical_flow_tracker_t *tracker);

/**
 * @brief 检测图像中的角点特征
 * @param image 输入图像（灰度）
 * @param width 图像宽度
 * @param height 图像高度
 * @param features 输出特征点数组
 * @param max_features 最大特征点数量
 * @return 检测到的特征点数量
 */
int detect_corners(const uint8_t *image, int width, int height, 
                   feature_point_t *features, int max_features);

/**
 * @brief 计算两帧之间的光流
 * @param prev_image 前一帧图像
 * @param curr_image 当前帧图像
 * @param width 图像宽度
 * @param height 图像高度
 * @param features 特征点数组
 * @param flows 输出光流向量数组
 * @param feature_count 特征点数量
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t calculate_optical_flow(const uint8_t *prev_image, const uint8_t *curr_image,
                                 int width, int height,
                                 const feature_point_t *features, optical_flow_t *flows,
                                 int feature_count);

/**
 * @brief 更新光流跟踪器
 * @param tracker 光流跟踪器指针
 * @param frame 当前帧图像缓冲区
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t optical_flow_update(optical_flow_tracker_t *tracker, camera_fb_t *frame);

/**
 * @brief 估计整体运动
 * @param tracker 光流跟踪器指针
 * @param estimate 输出运动估计结果
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t estimate_motion(const optical_flow_tracker_t *tracker, motion_estimate_t *estimate);

/**
 * @brief 将RGB565转换为灰度图像
 * @param rgb565_data RGB565数据
 * @param gray_data 输出灰度数据
 * @param width 图像宽度
 * @param height 图像高度
 */
void rgb565_to_gray(const uint16_t *rgb565_data, uint8_t *gray_data, int width, int height);

/**
 * @brief 在图像上绘制光流向量
 * @param frame 图像帧缓冲区
 * @param tracker 光流跟踪器
 * @param color 绘制颜色（RGB565）
 */
void draw_optical_flow(camera_fb_t *frame, const optical_flow_tracker_t *tracker, uint16_t color);

/**
 * @brief 光流跟踪测试函数
 */
void optical_flow_test(void);

#endif // OPTICAL_FLOW_H