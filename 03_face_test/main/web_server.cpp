#include "web_server.hpp"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include <string.h>

// 增加HTTP请求头缓冲区大小以解决"request URI/header too long"错误
#ifdef CONFIG_HTTPD_MAX_REQ_HDR_LEN
#undef CONFIG_HTTPD_MAX_REQ_HDR_LEN
#endif
#define CONFIG_HTTPD_MAX_REQ_HDR_LEN 2048

#ifdef HTTPD_MAX_REQ_HDR_LEN
#undef HTTPD_MAX_REQ_HDR_LEN
#endif
#define HTTPD_MAX_REQ_HDR_LEN 2048

static const char *TAG = "web_server";

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n";

// HTML页面内容
static const char* index_html = 
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"    <title>ESP32 人脸检测摄像头</title>\n"
"    <meta charset='utf-8'>\n"
"    <style>\n"
"        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f0f0f0; }\n"
"        .container { max-width: 800px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }\n"
"        h1 { color: #333; text-align: center; }\n"
"        .camera-container { text-align: center; margin: 20px 0; }\n"
"        img { max-width: 100%; height: auto; border: 2px solid #ddd; border-radius: 5px; }\n"
"        .info { background-color: #e7f3ff; padding: 15px; border-radius: 5px; margin: 20px 0; }\n"
"        .controls { text-align: center; margin: 20px 0; }\n"
"        button { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; }\n"
"        button:hover { background-color: #45a049; }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div class='container'>\n"
"        <h1>ESP32 人脸检测摄像头</h1>\n"
"        <div class='info'>\n"
"            <p><strong>实时视频流：</strong>下方显示摄像头实时画面，检测到的人脸会用方框标出</p>\n"
"            <p><strong>分辨率：</strong>240x240 RGB565格式</p>\n"
"            <p><strong>检测模型：</strong>HumanFaceDetectMSR01 + HumanFaceDetectMNP01</p>\n"
"        </div>\n"
"        <div class='camera-container'>\n"
"            <img id='stream' src='/stream' alt='摄像头视频流'>\n"
"        </div>\n"
"        <div class='controls'>\n"
"            <button onclick='location.reload()'>刷新页面</button>\n"
"            <button onclick='captureImage()'>拍照</button>\n"
"        </div>\n"
"    </div>\n"
"    <script>\n"
"        function captureImage() {\n"
"            window.open('/capture', '_blank');\n"
"        }\n"
"        // 检查图像加载状态\n"
"        document.getElementById('stream').onerror = function() {\n"
"            this.src = 'data:image/svg+xml;base64,PHN2ZyB3aWR0aD0nMjQwJyBoZWlnaHQ9JzI0MCcgeG1sbnM9J2h0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnJz48cmVjdCB3aWR0aD0nMTAwJScgaGVpZ2h0PScxMDAlJyBmaWxsPScjZGRkJy8+PHRleHQgeD0nNTAlJyB5PSc1MCUnIGZvbnQtZmFtaWx5PSdBcmlhbCcgZm9udC1zaXplPScxNCcgZmlsbD0nIzk5OScgdGV4dC1hbmNob3I9J21pZGRsZScgZHk9Jy4zZW0nPuaRhOWDj+WktOWksei0nSE8L3RleHQ+PC9zdmc+';\n"
"        };\n"
"    </script>\n"
"</body>\n"
"</html>";

// 主页处理函数
esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html, strlen(index_html));
}

// JPEG图像捕获处理函数
esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();
    int retry_count = 0;
    const int max_retries = 3;

    // 带重试机制的摄像头捕获
    while (retry_count < max_retries) {
        fb = esp_camera_fb_get();
        if (fb) {
            break;
        }
        retry_count++;
        ESP_LOGW(TAG, "摄像头捕获失败，重试 %d/%d", retry_count, max_retries);
        if (retry_count < max_retries) {
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms后重试
        }
    }
    
    if (!fb) {
        ESP_LOGE(TAG, "摄像头捕获最终失败");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t fb_len = 0;
    if(fb->format == PIXFORMAT_JPEG){
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    } else {
        // 将RGB565转换为JPEG
        uint8_t * jpg_buf = NULL;
        size_t jpg_buf_len = 0;
        bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
        if(!jpeg_converted){
            ESP_LOGE(TAG, "JPEG转换失败");
            esp_camera_fb_return(fb);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        res = httpd_resp_send(req, (const char *)jpg_buf, jpg_buf_len);
        free(jpg_buf);
        fb_len = jpg_buf_len;
    }
    esp_camera_fb_return(fb);
    int64_t fr_end = esp_timer_get_time();
    ESP_LOGI(TAG, "JPG: %luKB %lums", (uint32_t)(fb_len/1024), (uint32_t)((fr_end - fr_start)/1000));
    return res;
}

// MJPEG流处理函数
esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t jpg_buf_len = 0;
    uint8_t * jpg_buf = NULL;
    char part_buf[64];
    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while(true){
        // 带重试机制的摄像头捕获
        int retry_count = 0;
        const int max_retries = 2;
        
        while (retry_count < max_retries) {
            fb = esp_camera_fb_get();
            if (fb) {
                break;
            }
            retry_count++;
            if (retry_count < max_retries) {
                vTaskDelay(pdMS_TO_TICKS(50)); // 等待50ms后重试
            }
        }
        
        if (!fb) {
            ESP_LOGW(TAG, "摄像头捕获失败，跳过此帧");
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms后继续
            continue;
        }
        
        if (false) { // 保持原有的错误处理结构
            res = ESP_FAIL;
            break;
        }
        
        if(fb->format != PIXFORMAT_JPEG){
            bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
            if(!jpeg_converted){
                ESP_LOGE(TAG, "JPEG压缩失败");
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
                break;
            }
        } else {
            jpg_buf_len = fb->len;
            jpg_buf = fb->buf;
        }

        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(res == ESP_OK){
            int hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, jpg_buf_len);
            if(hlen < 0 || hlen >= sizeof(part_buf)){
                ESP_LOGE(TAG, "Header truncated (%d bytes needed >= %zu buffer)",
                         hlen, sizeof(part_buf));
                res = ESP_FAIL;
            } else {
                res = httpd_resp_send_chunk(req, part_buf, (size_t)hlen);
            }
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_buf_len);
        }
        if(fb->format != PIXFORMAT_JPEG){
            free(jpg_buf);
        }
        esp_camera_fb_return(fb);
        if(res != ESP_OK){
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        float fps = frame_time > 0 ? 1000.0f / (float)frame_time : 0.0f;
        ESP_LOGI(TAG, "MJPG: %luKB %lums (%.1ffps)",
            (uint32_t)(jpg_buf_len/1024),
            (uint32_t)frame_time, fps);
    }

    last_frame = 0;
    return res;
}

// 启动Web服务器
esp_err_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;
    config.max_resp_headers = 8;
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_open_sockets = 7;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;
    // 增加响应头数量限制
    config.max_resp_headers = 16;

    // 启动HTTP服务器
    ESP_LOGI(TAG, "启动HTTP服务器，端口: %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // 注册URI处理程序
        httpd_uri_t index_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = index_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t capture_uri = {
            .uri       = "/capture",
            .method    = HTTP_GET,
            .handler   = capture_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &capture_uri);

        httpd_uri_t stream_uri = {
            .uri       = "/stream",
            .method    = HTTP_GET,
            .handler   = stream_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &stream_uri);

        ESP_LOGI(TAG, "Web服务器启动成功！");
        ESP_LOGI(TAG, "访问 http://ESP32_IP_ADDRESS 查看摄像头画面");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "启动HTTP服务器失败");
    return ESP_FAIL;
}