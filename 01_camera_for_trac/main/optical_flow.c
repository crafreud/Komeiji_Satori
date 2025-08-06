#include "optical_flow.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <math.h>

static const char *TAG = "OPTICAL_FLOW";

/**
 * @brief 计算图像梯度
 */
static void compute_gradients(const uint8_t *image, int width, int height,
                             float *grad_x, float *grad_y) {
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int idx = y * width + x;
            
            // Sobel算子计算梯度
            grad_x[idx] = (image[y * width + (x + 1)] - image[y * width + (x - 1)]) / 2.0f;
            grad_y[idx] = (image[(y + 1) * width + x] - image[(y - 1) * width + x]) / 2.0f;
        }
    }
}

/**
 * @brief 计算Harris角点响应
 */
static float harris_response(const float *grad_x, const float *grad_y, 
                            int x, int y, int width, int height) {
    if (x < 2 || x >= width - 2 || y < 2 || y >= height - 2) {
        return 0.0f;
    }
    
    float A = 0, B = 0, C = 0;
    
    // 在3x3窗口内计算结构张量
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int idx = (y + dy) * width + (x + dx);
            float gx = grad_x[idx];
            float gy = grad_y[idx];
            
            A += gx * gx;
            B += gx * gy;
            C += gy * gy;
        }
    }
    
    // Harris响应函数
    float det = A * C - B * B;
    float trace = A + C;
    float k = 0.04f;
    
    return det - k * trace * trace;
}

esp_err_t optical_flow_init(optical_flow_tracker_t *tracker, int width, int height) {
    if (!tracker) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(tracker, 0, sizeof(optical_flow_tracker_t));
    
    tracker->width = width;
    tracker->height = height;
    
    // 分配前一帧和当前帧的内存
    tracker->prev_frame = heap_caps_malloc(width * height, MALLOC_CAP_8BIT);
    tracker->curr_frame = heap_caps_malloc(width * height, MALLOC_CAP_8BIT);
    
    if (!tracker->prev_frame || !tracker->curr_frame) {
        optical_flow_deinit(tracker);
        return ESP_ERR_NO_MEM;
    }
    
    tracker->initialized = false;
    tracker->feature_count = 0;
    
    ESP_LOGI(TAG, "光流跟踪器初始化成功，图像尺寸: %dx%d", width, height);
    return ESP_OK;
}

void optical_flow_deinit(optical_flow_tracker_t *tracker) {
    if (!tracker) {
        return;
    }
    
    if (tracker->prev_frame) {
        free(tracker->prev_frame);
        tracker->prev_frame = NULL;
    }
    
    if (tracker->curr_frame) {
        free(tracker->curr_frame);
        tracker->curr_frame = NULL;
    }
    
    tracker->initialized = false;
    ESP_LOGI(TAG, "光流跟踪器资源已释放");
}

int detect_corners(const uint8_t *image, int width, int height, 
                   feature_point_t *features, int max_features) {
    if (!image || !features || max_features <= 0) {
        return 0;
    }
    
    // 分配梯度计算内存
    float *grad_x = malloc(width * height * sizeof(float));
    float *grad_y = malloc(width * height * sizeof(float));
    
    if (!grad_x || !grad_y) {
        if (grad_x) free(grad_x);
        if (grad_y) free(grad_y);
        return 0;
    }
    
    memset(grad_x, 0, width * height * sizeof(float));
    memset(grad_y, 0, width * height * sizeof(float));
    
    // 计算图像梯度
    compute_gradients(image, width, height, grad_x, grad_y);
    
    int feature_count = 0;
    
    // 在图像中搜索角点
    for (int y = 10; y < height - 10; y += 8) {  // 降低搜索密度
        for (int x = 10; x < width - 10; x += 8) {
            if (feature_count >= max_features) {
                break;
            }
            
            float response = harris_response(grad_x, grad_y, x, y, width, height);
            
            if (response > MIN_EIGENVALUE) {
                features[feature_count].x = x;
                features[feature_count].y = y;
                features[feature_count].response = response;
                features[feature_count].valid = true;
                feature_count++;
            }
        }
        if (feature_count >= max_features) {
            break;
        }
    }
    
    free(grad_x);
    free(grad_y);
    
    ESP_LOGD(TAG, "检测到 %d 个角点特征", feature_count);
    return feature_count;
}

esp_err_t calculate_optical_flow(const uint8_t *prev_image, const uint8_t *curr_image,
                                 int width, int height,
                                 const feature_point_t *features, optical_flow_t *flows,
                                 int feature_count) {
    if (!prev_image || !curr_image || !features || !flows) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 为每个特征点计算光流
    for (int i = 0; i < feature_count; i++) {
        if (!features[i].valid) {
            flows[i].tracked = false;
            continue;
        }
        
        int fx = (int)features[i].x;
        int fy = (int)features[i].y;
        
        // 检查边界
        if (fx < WINDOW_SIZE || fx >= width - WINDOW_SIZE ||
            fy < WINDOW_SIZE || fy >= height - WINDOW_SIZE) {
            flows[i].tracked = false;
            continue;
        }
        
        // Lucas-Kanade光流计算
        float A11 = 0, A12 = 0, A22 = 0;
        float b1 = 0, b2 = 0;
        
        // 在窗口内计算梯度和时间差分
        for (int dy = -WINDOW_SIZE/2; dy <= WINDOW_SIZE/2; dy++) {
            for (int dx = -WINDOW_SIZE/2; dx <= WINDOW_SIZE/2; dx++) {
                int x = fx + dx;
                int y = fy + dy;
                
                if (x < 1 || x >= width - 1 || y < 1 || y >= height - 1) {
                    continue;
                }
                
                // 计算空间梯度
                float Ix = (prev_image[y * width + (x + 1)] - prev_image[y * width + (x - 1)]) / 2.0f;
                float Iy = (prev_image[(y + 1) * width + x] - prev_image[(y - 1) * width + x]) / 2.0f;
                
                // 计算时间梯度
                float It = curr_image[y * width + x] - prev_image[y * width + x];
                
                // 累积矩阵元素
                A11 += Ix * Ix;
                A12 += Ix * Iy;
                A22 += Iy * Iy;
                b1 -= Ix * It;
                b2 -= Iy * It;
            }
        }
        
        // 求解线性方程组 A * [u, v]^T = b
        float det = A11 * A22 - A12 * A12;
        
        if (fabs(det) < 1e-6) {
            flows[i].tracked = false;
            continue;
        }
        
        flows[i].dx = (A22 * b1 - A12 * b2) / det;
        flows[i].dy = (A11 * b2 - A12 * b1) / det;
        flows[i].tracked = true;
        flows[i].error = sqrt(flows[i].dx * flows[i].dx + flows[i].dy * flows[i].dy);
    }
    
    return ESP_OK;
}

void rgb565_to_gray(const uint16_t *rgb565_data, uint8_t *gray_data, int width, int height) {
    for (int i = 0; i < width * height; i++) {
        uint16_t pixel = rgb565_data[i];
        
        // 提取RGB分量
        uint8_t r = (pixel >> 11) & 0x1F;
        uint8_t g = (pixel >> 5) & 0x3F;
        uint8_t b = pixel & 0x1F;
        
        // 转换为8位
        r = (r * 255) / 31;
        g = (g * 255) / 63;
        b = (b * 255) / 31;
        
        // 计算灰度值
        gray_data[i] = (uint8_t)(0.299f * r + 0.587f * g + 0.114f * b);
    }
}

esp_err_t optical_flow_update(optical_flow_tracker_t *tracker, camera_fb_t *frame) {
    if (!tracker || !frame || frame->format != PIXFORMAT_RGB565) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 转换当前帧为灰度图像
    rgb565_to_gray((uint16_t*)frame->buf, tracker->curr_frame, tracker->width, tracker->height);
    
    if (!tracker->initialized) {
        // 第一次初始化，检测特征点
        tracker->feature_count = detect_corners(tracker->curr_frame, tracker->width, tracker->height,
                                               tracker->features, MAX_FEATURES);
        
        // 复制当前帧到前一帧
        memcpy(tracker->prev_frame, tracker->curr_frame, tracker->width * tracker->height);
        tracker->initialized = true;
        
        ESP_LOGI(TAG, "光流跟踪器初始化完成，检测到 %d 个特征点", tracker->feature_count);
        return ESP_OK;
    }
    
    // 计算光流
    esp_err_t ret = calculate_optical_flow(tracker->prev_frame, tracker->curr_frame,
                                          tracker->width, tracker->height,
                                          tracker->features, tracker->flows,
                                          tracker->feature_count);
    
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 更新特征点位置
    int valid_features = 0;
    for (int i = 0; i < tracker->feature_count; i++) {
        if (tracker->flows[i].tracked && tracker->flows[i].error < 10.0f) {
            tracker->features[i].x += tracker->flows[i].dx;
            tracker->features[i].y += tracker->flows[i].dy;
            
            // 检查边界
            if (tracker->features[i].x < 10 || tracker->features[i].x >= tracker->width - 10 ||
                tracker->features[i].y < 10 || tracker->features[i].y >= tracker->height - 10) {
                tracker->features[i].valid = false;
            } else {
                valid_features++;
            }
        } else {
            tracker->features[i].valid = false;
        }
    }
    
    // 如果有效特征点太少，重新检测
    if (valid_features < MAX_FEATURES / 3) {
        tracker->feature_count = detect_corners(tracker->curr_frame, tracker->width, tracker->height,
                                               tracker->features, MAX_FEATURES);
        ESP_LOGD(TAG, "重新检测特征点，数量: %d", tracker->feature_count);
    }
    
    // 交换前一帧和当前帧
    uint8_t *temp = tracker->prev_frame;
    tracker->prev_frame = tracker->curr_frame;
    tracker->curr_frame = temp;
    
    return ESP_OK;
}

esp_err_t estimate_motion(const optical_flow_tracker_t *tracker, motion_estimate_t *estimate) {
    if (!tracker || !estimate) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(estimate, 0, sizeof(motion_estimate_t));
    
    if (!tracker->initialized) {
        return ESP_OK;
    }
    
    float sum_dx = 0, sum_dy = 0;
    int tracked_count = 0;
    
    // 计算平均光流向量
    for (int i = 0; i < tracker->feature_count; i++) {
        if (tracker->features[i].valid && tracker->flows[i].tracked) {
            sum_dx += tracker->flows[i].dx;
            sum_dy += tracker->flows[i].dy;
            tracked_count++;
        }
    }
    
    if (tracked_count > 0) {
        estimate->motion_x = sum_dx / tracked_count;
        estimate->motion_y = sum_dy / tracked_count;
        estimate->tracked_points = tracked_count;
        estimate->confidence = (float)tracked_count / MAX_FEATURES;
        
        // 判断是否检测到显著运动
        float motion_magnitude = sqrt(estimate->motion_x * estimate->motion_x + 
                                     estimate->motion_y * estimate->motion_y);
        estimate->motion_detected = motion_magnitude > 1.0f;
        
        ESP_LOGD(TAG, "运动估计: dx=%.2f, dy=%.2f, 跟踪点数=%d, 置信度=%.2f",
                estimate->motion_x, estimate->motion_y, tracked_count, estimate->confidence);
    }
    
    return ESP_OK;
}

void draw_optical_flow(camera_fb_t *frame, const optical_flow_tracker_t *tracker, uint16_t color) {
    if (!frame || !tracker || !tracker->initialized) {
        return;
    }
    
    uint16_t *pixels = (uint16_t *)frame->buf;
    int width = frame->width;
    int height = frame->height;
    
    // 绘制特征点和光流向量
    for (int i = 0; i < tracker->feature_count; i++) {
        if (!tracker->features[i].valid || !tracker->flows[i].tracked) {
            continue;
        }
        
        int x = (int)tracker->features[i].x;
        int y = (int)tracker->features[i].y;
        
        // 绘制特征点
        if (x >= 0 && x < width && y >= 0 && y < height) {
            pixels[y * width + x] = color;
            
            // 绘制十字标记
            for (int dx = -2; dx <= 2; dx++) {
                if (x + dx >= 0 && x + dx < width) {
                    pixels[y * width + (x + dx)] = color;
                }
            }
            for (int dy = -2; dy <= 2; dy++) {
                if (y + dy >= 0 && y + dy < height) {
                    pixels[(y + dy) * width + x] = color;
                }
            }
            
            // 绘制光流向量（缩放显示）
            int end_x = x + (int)(tracker->flows[i].dx * 5);
            int end_y = y + (int)(tracker->flows[i].dy * 5);
            
            if (end_x >= 0 && end_x < width && end_y >= 0 && end_y < height) {
                // 简单的线段绘制
                int steps = (int)sqrt((end_x - x) * (end_x - x) + (end_y - y) * (end_y - y));
                for (int step = 0; step <= steps; step++) {
                    int px = x + (end_x - x) * step / (steps + 1);
                    int py = y + (end_y - y) * step / (steps + 1);
                    if (px >= 0 && px < width && py >= 0 && py < height) {
                        pixels[py * width + px] = color;
                    }
                }
            }
        }
    }
}

void optical_flow_test(void) {
    ESP_LOGI(TAG, "开始光流算法测试");
    
    optical_flow_tracker_t tracker;
    if (optical_flow_init(&tracker, 320, 240) != ESP_OK) {
        ESP_LOGE(TAG, "光流跟踪器初始化失败");
        return;
    }
    
    ESP_LOGI(TAG, "光流算法测试完成");
    optical_flow_deinit(&tracker);
}