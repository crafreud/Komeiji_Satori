#include "camera_color_detection.h"
#include "optical_flow.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

static const char *TAG = "CAMERA_COLOR";

// 预定义颜色阈值
const color_threshold_t RED_THRESHOLD = {
    .r_min = 100, .r_max = 255,
    .g_min = 0,   .g_max = 80,
    .b_min = 0,   .b_max = 80
};

const color_threshold_t GREEN_THRESHOLD = {
    .r_min = 0,   .r_max = 80,
    .g_min = 100, .g_max = 255,
    .b_min = 0,   .b_max = 80
};

const color_threshold_t BLUE_THRESHOLD = {
    .r_min = 0,   .r_max = 80,
    .g_min = 0,   .g_max = 80,
    .b_min = 100, .b_max = 255
};

const color_threshold_t YELLOW_THRESHOLD = {
    .r_min = 150, .r_max = 255,
    .g_min = 150, .g_max = 255,
    .b_min = 0,   .b_max = 100
};

// 肤色阈值（适用于亚洲人肤色）
const color_threshold_t SKIN_THRESHOLD = {
    .r_min = 95,  .r_max = 255,
    .g_min = 40,  .g_max = 180,
    .b_min = 20,  .b_max = 120
};

// 静态摄像头配置
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
    .xclk_freq_hz = 10000000,  // 适中的时钟频率，兼容性更好
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_RGB565,  // 使用RGB565格式确保兼容性
    .frame_size = FRAMESIZE_QVGA,  // 320x240
    .jpeg_quality = 10,  // JPEG质量设置（RGB565模式下不使用）
    .fb_count = 2,  // 双缓冲提高性能
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,  // 获取最新帧
};

/**
 * @brief 初始化摄像头
 */
esp_err_t camera_init(void)
{
    // 带重试机制的摄像头初始化
    esp_err_t err;
    int retry_count = 0;
    const int max_retries = 5;
    
    while (retry_count < max_retries) {
        err = esp_camera_init(&camera_config);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "摄像头初始化成功");
            break;
        }
        
        retry_count++;
        ESP_LOGI(TAG, "摄像头初始化失败 (尝试 %d/%d): %s", retry_count, max_retries, esp_err_to_name(err));
        
        if (retry_count < max_retries) {
            ESP_LOGI(TAG, "等待8秒后重试...");
            vTaskDelay(pdMS_TO_TICKS(8000));
        }
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "摄像头初始化最终失败: %s", esp_err_to_name(err));
        return err;
    }

    // 获取传感器句柄
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        ESP_LOGE(TAG, "获取摄像头传感器失败");
        return ESP_FAIL;
    }

    // 设置摄像头参数
    s->set_brightness(s, 0);     // 亮度 (-2 到 2)
    s->set_contrast(s, 0);       // 对比度 (-2 到 2)
    s->set_saturation(s, 0);     // 饱和度 (-2 到 2)
    s->set_special_effect(s, 0); // 特效 (0 到 6)
    s->set_whitebal(s, 1);       // 白平衡开启
    s->set_awb_gain(s, 1);       // 自动白平衡增益开启
    s->set_wb_mode(s, 0);        // 白平衡模式 (0 到 4)
    s->set_exposure_ctrl(s, 1);  // 曝光控制开启
    s->set_aec2(s, 0);           // AEC2开启
    s->set_ae_level(s, 0);       // AE等级 (-2 到 2)
    s->set_aec_value(s, 300);    // AEC值 (0 到 1200)
    s->set_gain_ctrl(s, 1);      // 增益控制开启
    s->set_agc_gain(s, 0);       // AGC增益 (0 到 30)
    s->set_gainceiling(s, (gainceiling_t)0); // 增益上限
    s->set_bpc(s, 0);            // BPC开启
    s->set_wpc(s, 1);            // WPC开启
    s->set_raw_gma(s, 1);        // Raw GMA开启
    s->set_lenc(s, 1);           // 镜头校正开启
    s->set_hmirror(s, 0);        // 水平镜像
    s->set_vflip(s, 0);          // 垂直翻转
    s->set_dcw(s, 1);            // DCW开启
    s->set_colorbar(s, 0);       // 彩条测试关闭

    ESP_LOGI(TAG, "摄像头初始化成功");
    return ESP_OK;
}

/**
 * @brief 反初始化摄像头
 */
esp_err_t camera_deinit(void)
{
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "摄像头反初始化失败: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "摄像头反初始化成功");
    return ESP_OK;
}

/**
 * @brief 捕获一帧图像
 */
camera_fb_t* camera_capture(void)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "摄像头捕获失败");
        return NULL;
    }
    return fb;
}

/**
 * @brief 释放图像缓冲区
 */
void camera_fb_return(camera_fb_t *fb)
{
    if (fb) {
        esp_camera_fb_return(fb);
    }
}

/**
 * @brief RGB565转RGB888
 */
void rgb565_to_rgb888(uint16_t rgb565, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (rgb565 >> 11) << 3;           // 取高5位，左移3位
    *g = ((rgb565 >> 5) & 0x3F) << 2;   // 取中6位，左移2位
    *b = (rgb565 & 0x1F) << 3;          // 取低5位，左移3位
}

/**
 * @brief 检查像素是否在颜色阈值范围内
 */
bool is_color_in_threshold(uint8_t r, uint8_t g, uint8_t b, const color_threshold_t *threshold)
{
    return (r >= threshold->r_min && r <= threshold->r_max &&
            g >= threshold->g_min && g <= threshold->g_max &&
            b >= threshold->b_min && b <= threshold->b_max);
}

/**
 * @brief 在图像中检测指定颜色的色块
 */
esp_err_t detect_color_blob(camera_fb_t *fb, const color_threshold_t *threshold, color_blob_t *blob)
{
    if (!fb || !threshold || !blob) {
        return ESP_ERR_INVALID_ARG;
    }

    // 初始化色块信息
    blob->found = false;
    blob->x_center = 0;
    blob->y_center = 0;
    blob->width = 0;
    blob->height = 0;
    blob->area = 0;

    if (fb->format != PIXFORMAT_RGB565) {
        ESP_LOGE(TAG, "不支持的图像格式");
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint16_t *pixels = (uint16_t *)fb->buf;
    int width = fb->width;
    int height = fb->height;
    
    // 用于计算色块边界
    int min_x = width, max_x = 0;
    int min_y = height, max_y = 0;
    int pixel_count = 0;
    int sum_x = 0, sum_y = 0;

    // 遍历图像像素
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t pixel = pixels[y * width + x];
            uint8_t r, g, b;
            rgb565_to_rgb888(pixel, &r, &g, &b);

            // 检查像素是否在目标颜色范围内
            if (is_color_in_threshold(r, g, b, threshold)) {
                pixel_count++;
                sum_x += x;
                sum_y += y;
                
                // 更新边界
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
        
        // 每处理完一行就让其他任务有机会运行，避免看门狗超时
        if (y % 10 == 0) {
            taskYIELD();
        }
    }

    // 如果找到足够的像素点，认为找到了色块
    if (pixel_count > 100) {  // 最小像素阈值
        blob->found = true;
        blob->x_center = sum_x / pixel_count;
        blob->y_center = sum_y / pixel_count;
        blob->width = max_x - min_x + 1;
        blob->height = max_y - min_y + 1;
        blob->area = pixel_count;
    }

    return ESP_OK;
}

/**
 * @brief 在RGB565图像上绘制矩形框
 */
void draw_rectangle_rgb565(camera_fb_t *fb, int x, int y, int width, int height, uint16_t color, int thickness)
{
    if (!fb || fb->format != PIXFORMAT_RGB565) {
        return;
    }
    
    uint16_t *pixels = (uint16_t *)fb->buf;
    int img_width = fb->width;
    int img_height = fb->height;
    
    // 确保坐标在图像范围内
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + width > img_width) width = img_width - x;
    if (y + height > img_height) height = img_height - y;
    
    // 绘制矩形框
    for (int t = 0; t < thickness && t < height/2 && t < width/2; t++) {
        // 上边
        for (int i = x + t; i < x + width - t; i++) {
            if (y + t >= 0 && y + t < img_height && i >= 0 && i < img_width) {
                pixels[(y + t) * img_width + i] = color;
            }
        }
        // 下边
        for (int i = x + t; i < x + width - t; i++) {
            if (y + height - 1 - t >= 0 && y + height - 1 - t < img_height && i >= 0 && i < img_width) {
                pixels[(y + height - 1 - t) * img_width + i] = color;
            }
        }
        // 左边
        for (int j = y + t; j < y + height - t; j++) {
            if (j >= 0 && j < img_height && x + t >= 0 && x + t < img_width) {
                pixels[j * img_width + (x + t)] = color;
            }
        }
        // 右边
        for (int j = y + t; j < y + height - t; j++) {
            if (j >= 0 && j < img_height && x + width - 1 - t >= 0 && x + width - 1 - t < img_width) {
                pixels[j * img_width + (x + width - 1 - t)] = color;
            }
        }
    }
}

/**
 * @brief 在图像中检测指定颜色的色块并绘制轮廓
 */
esp_err_t detect_and_draw_color_blob(camera_fb_t *fb, const color_threshold_t *threshold, color_blob_t *blob, uint16_t draw_color)
{
    if (!fb || !threshold || !blob) {
        return ESP_ERR_INVALID_ARG;
    }

    // 先进行色块检测
    esp_err_t ret = detect_color_blob(fb, threshold, blob);
    if (ret != ESP_OK) {
        return ret;
    }

    // 如果检测到色块，绘制矩形框
    if (blob->found) {
        int rect_x = blob->x_center - blob->width / 2;
        int rect_y = blob->y_center - blob->height / 2;
        
        // 增加矩形框厚度，使其更明显
        draw_rectangle_rgb565(fb, rect_x, rect_y, blob->width, blob->height, draw_color, 3);
        
        // 绘制更大的中心点
        int center_size = 8;
        draw_rectangle_rgb565(fb, blob->x_center - center_size/2, blob->y_center - center_size/2, 
                            center_size, center_size, draw_color, 2);
        
        // 添加调试信息
        ESP_LOGI(TAG, "绘制色块轮廓: 位置(%d,%d), 大小(%dx%d), 颜色0x%04X", 
                 rect_x, rect_y, blob->width, blob->height, draw_color);
    }

    return ESP_OK;
}

/**
 * @brief 打印色块信息
 */
void print_blob_info(const color_blob_t *blob, const char *color_name)
{
    if (blob->found) {
        printf("%s色块检测结果:\n", color_name);
        printf("  中心坐标: (%d, %d)\n", blob->x_center, blob->y_center);
        printf("  尺寸: %dx%d\n", blob->width, blob->height);
        printf("  面积: %d像素\n", blob->area);
    } else {
        printf("未检测到%s色块\n", color_name);
    }
}

/**
 * @brief 摄像头色块检测测试函数
 */
void camera_color_detection_test(void)
{
    ESP_LOGI(TAG, "开始摄像头色块检测测试...");
    
    while (1) {
        // 捕获图像
        camera_fb_t *fb = camera_capture();
        if (!fb) {
            ESP_LOGE(TAG, "图像捕获失败");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        printf("\n=== 图像信息 ===\n");
        printf("分辨率: %dx%d\n", fb->width, fb->height);
        printf("格式: %d\n", fb->format);
        printf("大小: %zu bytes\n", fb->len);

        // 检测红色色块
        color_blob_t red_blob;
        if (detect_color_blob(fb, &RED_THRESHOLD, &red_blob) == ESP_OK) {
            print_blob_info(&red_blob, "红色");
        }

        // 检测绿色色块
        color_blob_t green_blob;
        if (detect_color_blob(fb, &GREEN_THRESHOLD, &green_blob) == ESP_OK) {
            print_blob_info(&green_blob, "绿色");
        }

        // 检测蓝色色块
        color_blob_t blue_blob;
        if (detect_color_blob(fb, &BLUE_THRESHOLD, &blue_blob) == ESP_OK) {
            print_blob_info(&blue_blob, "蓝色");
        }

        // 检测黄色色块
        color_blob_t yellow_blob;
        if (detect_color_blob(fb, &YELLOW_THRESHOLD, &yellow_blob) == ESP_OK) {
            print_blob_info(&yellow_blob, "黄色");
        }

        // 释放图像缓冲区
        camera_fb_return(fb);

        // 延时
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * @brief 基于光流向量创建运动掩码
 */
esp_err_t create_motion_mask(const optical_flow_tracker_t *tracker, uint8_t *motion_mask, int width, int height, float motion_threshold)
{
    if (!tracker || !motion_mask || width <= 0 || height <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 初始化掩码为0（无运动）
    memset(motion_mask, 0, width * height);

    // 如果跟踪器未初始化，返回
    if (!tracker->initialized || tracker->feature_count == 0) {
        return ESP_OK;
    }

    // 遍历所有有效的光流向量
    for (int i = 0; i < tracker->feature_count; i++) {
        const optical_flow_t *flow = &tracker->flows[i];
        const feature_point_t *feature = &tracker->features[i];
        
        if (!flow->tracked || !feature->valid) {
            continue;
        }

        // 计算运动幅度
        float motion_magnitude = sqrtf(flow->dx * flow->dx + flow->dy * flow->dy);
        
        if (motion_magnitude > motion_threshold) {
            // 在特征点周围创建运动区域
            int center_x = (int)feature->x;
            int center_y = (int)feature->y;
            int radius = 15; // 运动区域半径
            
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int x = center_x + dx;
                    int y = center_y + dy;
                    
                    // 检查边界
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        // 使用圆形区域
                        if (dx * dx + dy * dy <= radius * radius) {
                            motion_mask[y * width + x] = 255; // 标记为运动区域
                        }
                    }
                }
            }
        }
    }

    return ESP_OK;
}

/**
 * @brief 将运动区域的像素替换为白色
 */
esp_err_t replace_motion_pixels_with_white(camera_fb_t *fb, const uint8_t *motion_mask)
{
    if (!fb || !motion_mask || !fb->buf) {
        return ESP_ERR_INVALID_ARG;
    }

    // 确保是RGB565格式
    if (fb->format != PIXFORMAT_RGB565) {
        ESP_LOGE(TAG, "不支持的图像格式，需要RGB565");
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint16_t *pixel_data = (uint16_t *)fb->buf;
    int width = fb->width;
    int height = fb->height;
    uint16_t white_color = 0xFFFF; // RGB565格式的白色

    // 遍历所有像素
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = y * width + x;
            
            // 如果掩码标记为运动区域，替换为白色
            if (motion_mask[index] > 0) {
                pixel_data[index] = white_color;
            }
        }
    }

    return ESP_OK;
}

/**
 * @brief 检测运动区域并替换为白色
 */
esp_err_t detect_and_replace_motion_with_white(camera_fb_t *fb, const optical_flow_tracker_t *tracker, float motion_threshold)
{
    if (!fb || !tracker) {
        return ESP_ERR_INVALID_ARG;
    }

    int width = fb->width;
    int height = fb->height;
    
    // 分配运动掩码内存
    uint8_t *motion_mask = (uint8_t *)malloc(width * height);
    if (!motion_mask) {
        ESP_LOGE(TAG, "运动掩码内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    
    // 创建运动掩码
    ret = create_motion_mask(tracker, motion_mask, width, height, motion_threshold);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建运动掩码失败");
        goto cleanup;
    }

    // 替换运动区域为白色
    ret = replace_motion_pixels_with_white(fb, motion_mask);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "替换运动像素失败");
        goto cleanup;
    }

    // 统计运动像素数量（用于调试）
    int motion_pixel_count = 0;
    for (int i = 0; i < width * height; i++) {
        if (motion_mask[i] > 0) {
            motion_pixel_count++;
        }
    }
    
    if (motion_pixel_count > 0) {
        ESP_LOGI(TAG, "检测到运动，替换了 %d 个像素为白色", motion_pixel_count);
    }

cleanup:
    free(motion_mask);
    return ret;
}