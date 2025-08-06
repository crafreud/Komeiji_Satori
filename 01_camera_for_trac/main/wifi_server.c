#include "wifi_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_camera.h"
#include "camera_color_detection.h"
#include "optical_flow.h"
#include "esp_timer.h"
#include <esp_http_server.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "wifi_server";
static httpd_handle_t server = NULL;
static optical_flow_tracker_t stream_optical_flow_tracker;
static bool stream_use_optical_flow = false;

// HTML页面内容
static const char* index_html = 
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"    <title>ESP32摄像头色块跟踪</title>\n"
"    <meta charset='UTF-8'>\n"
"    <style>\n"
"        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }\n"
"        .container { max-width: 800px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }\n"
"        h1 { color: #333; text-align: center; }\n"
"        .video-container { text-align: center; margin: 20px 0; }\n"
"        img { max-width: 100%; border: 2px solid #ddd; border-radius: 5px; }\n"
"        .info { background-color: #e7f3ff; padding: 15px; border-radius: 5px; margin: 10px 0; }\n"
"        .status { font-weight: bold; color: #007acc; }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div class='container'>\n"
"        <h1>👆 ESP32摄像头运动跟踪与替换系统</h1>\n"
"        <div class='info'>\n"
"            <p><span class='status'>状态:</span> 实时视频流、手指跟踪与运动替换</p>\n"
"            <p><span class='status'>分辨率:</span> 320x240</p>\n"
"            <p><span class='status'>帧率:</span> ~10-20 FPS (优化MJPEG流)</p>\n"
"        </div>\n"
"        <div class='video-container'>\n"
"            <img id='stream' src='/stream' alt='摄像头视频流'>\n"
"        </div>\n"
"        <div class='info'>\n"
"            <p>🎯 系统功能：</p>\n"
"            <div style='display: flex; justify-content: center; margin: 15px 0;'>\n"
"                <div style='background: linear-gradient(45deg, #ffb366, #ff8533); color: white; padding: 15px; border-radius: 8px; min-width: 200px; text-align: center; box-shadow: 0 2px 4px rgba(0,0,0,0.2);'>\n"
"                    <strong>👆 手指检测 + 🔄 运动替换</strong><br>\n"
"                    <small>肤色阈值范围：<br>R: 95-255<br>G: 40-180<br>B: 20-120</small>\n"
"                </div>\n"
"            </div>\n"
"            <p>🎮 舵机会根据检测到的手指位置进行跟踪</p>\n            <p>⚪ 运动物体会被实时替换为白色</p>\n"
"            <p>🔍 检测到的手指会显示黄色矩形框</p>\n"
"            <p>💡 提示：移动手指或物体，观察运动区域被替换为白色</p>\n"
"        </div>\n"
"    </div>\n"
"    <script>\n"
"        // 使用MJPEG流，无需手动刷新\n"
"    </script>\n"
"</body>\n"
"</html>";

esp_err_t wifi_init_softap(void)
{
    ESP_LOGI(TAG, "开始初始化WiFi热点...");
    
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    // WiFi初始化配置
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // WiFi热点配置
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi热点启动成功");
    ESP_LOGI(TAG, "SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "密码: %s", WIFI_PASS);
    ESP_LOGI(TAG, "访问地址: http://192.168.4.1");
    
    return ESP_OK;
}

esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    return httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
}

// JPEG编码流处理结构体
typedef struct {
    httpd_req_t *req;
    size_t len;
} jpg_chunking_t;

// JPEG编码流回调函数
static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}

// 静态变量用于控制色块检测频率
static int frame_count = 0;

esp_err_t camera_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t fb_len = 0;
    int64_t fr_start = esp_timer_get_time();
    
    // 捕获图像
    fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "摄像头捕获失败");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 每5帧进行一次色块检测以提高图传速度
    frame_count++;
    if (frame_count % 5 == 0) {
        color_blob_t blob;
        
        // 定义RGB565格式的明亮绘制颜色 - 使用白色更明显
        uint16_t white_color = 0xFFFF;  // 白色 (RGB565: 11111 111111 11111) - 最明显
        uint16_t yellow_color = 0xFFE0; // 黄色 (RGB565: 11111 111111 00000) - 备选
        
        // 只检测并绘制红色色块 - 使用白色轮廓更明显
        esp_err_t detect_result = detect_and_draw_color_blob(fb, &RED_THRESHOLD, &blob, white_color);
        if (detect_result == ESP_OK && blob.found) {
            ESP_LOGI(TAG, "检测到红色色块: 中心(%d,%d), 大小(%dx%d), 面积%d", 
                     blob.x_center, blob.y_center, blob.width, blob.height, blob.area);
        } else if (detect_result == ESP_OK) {
            ESP_LOGD(TAG, "未检测到红色色块");
        }
    }

    // 设置HTTP响应头
    res = httpd_resp_set_type(req, "image/jpeg");
    if(res == ESP_OK){
        res = httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    }
    if(res == ESP_OK){
        res = httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    }
    if(res == ESP_OK){
        res = httpd_resp_set_hdr(req, "Pragma", "no-cache");
    }
    if(res == ESP_OK){
        res = httpd_resp_set_hdr(req, "Expires", "0");
    }
    
    // 发送图像数据
    if(res == ESP_OK){
        if(fb->format == PIXFORMAT_JPEG){
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        } else {
            jpg_chunking_t jchunk = {req, 0};
            res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk) ? ESP_OK : ESP_FAIL;
            httpd_resp_send_chunk(req, NULL, 0);
            fb_len = jchunk.len;
        }
    }
    
    // 释放帧缓冲
    esp_camera_fb_return(fb);
    
    int64_t fr_end = esp_timer_get_time();
    ESP_LOGI(TAG, "JPG: %luKB %lums", (unsigned long)(fb_len/1024), (unsigned long)((fr_end - fr_start)/1000));
    
    return res;
}

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// MJPEG流处理函数
esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t *_jpg_buf;
    char *part_buf[64];
    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }
        
        // 在MJPEG流中进行手指检测、运动检测和颜色替换
        static int stream_frame_count = 0;
        static bool stream_optical_flow_initialized = false;
        
        stream_frame_count++;
        
        // 初始化光流跟踪器（仅一次）
        if (!stream_optical_flow_initialized) {
            if (optical_flow_init(&stream_optical_flow_tracker, 320, 240) == ESP_OK) {
                stream_use_optical_flow = true;
                stream_optical_flow_initialized = true;
                ESP_LOGI(TAG, "视频流光流跟踪器初始化成功");
            } else {
                stream_use_optical_flow = false;
                ESP_LOGW(TAG, "视频流光流跟踪器初始化失败");
            }
        }
        
        // 运动检测和颜色替换
        if (stream_use_optical_flow) {
            if (optical_flow_update(&stream_optical_flow_tracker, fb) == ESP_OK) {
                motion_estimate_t motion;
                if (estimate_motion(&stream_optical_flow_tracker, &motion) == ESP_OK) {
                    if (motion.motion_detected) {
                        // 检测运动区域并替换为白色
                        float motion_threshold = 2.0f;
                        detect_and_replace_motion_with_white(fb, &stream_optical_flow_tracker, motion_threshold);
                        ESP_LOGD(TAG, "视频流中检测到运动，已替换为白色");
                    }
                }
            }
        }
        
        // 手指检测和绘制（每3帧一次）
        if (stream_frame_count % 3 == 0) {
            color_blob_t finger_blob;
            
            // 定义RGB565格式的明亮绘制颜色 - 使用黄色更明显
            uint16_t yellow_color = 0xFFE0;  // 黄色 (RGB565: 11111 111110 00000)
             
            // 检测并绘制手指（肤色）- 使用黄色轮廓
            esp_err_t detect_result = detect_and_draw_color_blob(fb, &SKIN_THRESHOLD, &finger_blob, yellow_color);
            if (detect_result == ESP_OK && finger_blob.found && 
                finger_blob.area > 50 && finger_blob.area < 5000) {  // 添加面积过滤
                ESP_LOGD(TAG, "流中检测到手指: 中心(%d,%d), 大小(%dx%d), 面积%d", 
                         finger_blob.x_center, finger_blob.y_center, 
                         finger_blob.width, finger_blob.height, finger_blob.area);
            }
        }
        
        if(fb->format != PIXFORMAT_JPEG){
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if(!jpeg_converted){
                ESP_LOGE(TAG, "JPEG compression failed");
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(fb->format != PIXFORMAT_JPEG){
            free(_jpg_buf);
        }
        esp_camera_fb_return(fb);
        if(res != ESP_OK){
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        ESP_LOGI(TAG, "MJPG: %luKB %lums (%.1ffps)", 
            (unsigned long)(_jpg_buf_len/1024), 
            (unsigned long)frame_time, 1000.0 / (unsigned long)frame_time);
    }

    last_frame = 0;
    return res;
}

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = SERVER_PORT;
    config.max_uri_handlers = 8;
    config.max_resp_headers = 8;
    config.stack_size = 8192;
    
    ESP_LOGI(TAG, "启动HTTP服务器，端口: %d", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // 注册URI处理程序
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index_uri);
        
        httpd_uri_t camera_uri = {
            .uri = "/camera",
            .method = HTTP_GET,
            .handler = camera_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &camera_uri);
        
        httpd_uri_t stream_uri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = stream_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &stream_uri);
        
        ESP_LOGI(TAG, "HTTP服务器启动成功");
        return server;
    }
    
    ESP_LOGE(TAG, "HTTP服务器启动失败");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    if (server) {
        httpd_stop(server);
        ESP_LOGI(TAG, "HTTP服务器已停止");
    }
}