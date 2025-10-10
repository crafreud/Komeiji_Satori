#include <stdio.h>
#include <string.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
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
        .clk_cfg = LEDC_AUTO_CLK};
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
            .duty = 0};
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
}

// ---------------- 人脸检测任务 ----------------
// 舵机角度范围
#define SERVO_H_MIN 50
#define SERVO_H_MAX 120
#define SERVO_V_MIN 90
#define SERVO_V_MAX 180

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
#define SERVO_V_MIN 90
#define SERVO_V_MAX 180

// static const char *TAG = "face_pid";

// ---------------- 简单 PID 结构体 ----------------
typedef struct {
    float Kp;
    float Ki;
    float Kd;
    float prev_error;
    float integral;
} PID_t;

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

// ---------------- 舵机控制函数接口 ----------------
// 这里假设你已有函数：void servo_set_angle(int servo_id, int angle);

void task_face_detect(void *arg)
{
    HumanFaceDetectMSR01 detector(0.3F, 0.3F, 10, 0.3F);
    HumanFaceDetectMNP01 detector2(0.4F, 0.3F, 10);

    // PID 初始化
    PID_t pid_h = {0.6f, 0.01f, 0.1f, 0.0f, 0.0f};
    PID_t pid_v = {0.6f, 0.01f, 0.1f, 0.0f, 0.0f};

    int servo_h_angle = 120; // 初始水平角
    int servo_v_angle = 70; // 初始垂直角

    while (1)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        std::list<dl::detect::result_t> &candidates = detector.infer(
            (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3});
        std::list<dl::detect::result_t> &results = detector2.infer(
            (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3}, candidates);

        if (!results.empty())
        {
            auto face = results.front(); // 取第一个人脸
            int xmin = face.box[0];
            int ymin = face.box[1];
            int xmax = face.box[2];
            int ymax = face.box[3];

            int cx = (xmin + xmax) / 2;
            int cy = (ymin + ymax) / 2;

            int img_w = fb->width;
            int img_h = fb->height;

            // ---------------- 目标角度映射 ----------------
            float target_h_angle = SERVO_H_MIN + (1.0f - (float)cx / img_w) * (SERVO_H_MAX - SERVO_H_MIN);
            float target_v_angle = SERVO_V_MIN + ((float)cy / img_h) * (SERVO_V_MAX - SERVO_V_MIN);

            // ---------------- PID 输出增量 ----------------
            float delta_h = pid_compute(&pid_h, target_h_angle, servo_h_angle);
            float delta_v = pid_compute(&pid_v, target_v_angle, servo_v_angle);

            // 更新舵机角度
            servo_h_angle += (int)delta_h;
            servo_v_angle += (int)delta_v;

            // 限制舵机角度在范围内
            if(servo_h_angle < SERVO_H_MIN) servo_h_angle = SERVO_H_MIN;
            if(servo_h_angle > SERVO_H_MAX) servo_h_angle = SERVO_H_MAX;
            if(servo_v_angle < SERVO_V_MIN) servo_v_angle = SERVO_V_MIN;
            if(servo_v_angle > SERVO_V_MAX) servo_v_angle = SERVO_V_MAX;

            // ---------------- 发送舵机 ----------------
            servo_set_angle(3, servo_h_angle); // 水平舵机
            servo_set_angle(2, servo_v_angle); // 垂直舵机

            ESP_LOGI(TAG, "Face center: (%d,%d) -> servo H: %d, V: %d", cx, cy, servo_h_angle, servo_v_angle);
        }

        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
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
    servo_set_angle(2, 90);
    servo_set_angle(3, 90);

    // 启动人脸检测任务
    xTaskCreate(task_face_detect, "face_detect", 8 * 1024, NULL, 5, NULL);
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
