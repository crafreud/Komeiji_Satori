/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "web_server.h"
#include "servo_control.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

// MJPEG流相关常量
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n";

// HTML页面内容
static const char* html_page = 
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"    <meta charset='UTF-8'>\n"
"    <title>舵机控制面板</title>\n"
"    <style>\n"
"        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }\n"
"        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }\n"
"        h1 { text-align: center; color: #333; }\n"
"        .servo-control { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }\n"
"        .servo-title { font-weight: bold; margin-bottom: 10px; color: #555; }\n"
"        .slider-container { display: flex; align-items: center; gap: 10px; }\n"
"        .slider { flex: 1; height: 6px; border-radius: 3px; background: #ddd; outline: none; }\n"
"        .angle-display { min-width: 50px; font-weight: bold; color: #333; }\n"
"        .button-group { text-align: center; margin: 20px 0; }\n"
"        button { padding: 10px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 14px; }\n"
"        .reset-btn { background-color: #4CAF50; color: white; }\n"
"        .reset-btn:hover { background-color: #45a049; }\n"
"        .status { text-align: center; margin: 10px 0; padding: 10px; border-radius: 5px; }\n"
"        .status.success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }\n"
"        .status.error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }\n"
"        .camera-section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; text-align: center; }\n"
"        .camera-title { font-weight: bold; margin-bottom: 15px; color: #555; }\n"
"        .camera-stream { max-width: 100%; height: auto; border: 2px solid #ddd; border-radius: 5px; }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div class='container'>\n"
"        <h1>🎛️ 舵机控制面板</h1>\n"
"        <div id='status' class='status' style='display:none;'></div>\n"
"        \n"
"        <div class='camera-section'>\n"
"            <div class='camera-title'>📷 实时图像监控</div>\n"
"            <img id='camera-stream' class='camera-stream' src='/stream' alt='摄像头图像流'>\n"
"        </div>\n"
"        \n"
"        <div class='servo-control'>\n"
"            <div class='servo-title'>舵机 1 (GPIO 1) - 范围: 50°-120°</div>\n"
"            <div class='slider-container'>\n"
"                <input type='range' min='50' max='120' value='85' class='slider' id='servo1' oninput='updateAngle(1, this.value)'>\n"
"                <span class='angle-display' id='angle1'>85°</span>\n"
"            </div>\n"
"        </div>\n"
"        \n"
"        <div class='servo-control'>\n"
"            <div class='servo-title'>舵机 2 (GPIO 2) - 范围: 90°-180°</div>\n"
"            <div class='slider-container'>\n"
"                <input type='range' min='90' max='180' value='135' class='slider' id='servo2' oninput='updateAngle(2, this.value)'>\n"
"                <span class='angle-display' id='angle2'>135°</span>\n"
"            </div>\n"
"        </div>\n"
"        \n"
"        <div class='servo-control'>\n"
"            <div class='servo-title'>舵机 3 (GPIO 4) - 范围: 50°-120°</div>\n"
"            <div class='slider-container'>\n"
"                <input type='range' min='50' max='120' value='85' class='slider' id='servo3' oninput='updateAngle(3, this.value)'>\n"
"                <span class='angle-display' id='angle3'>85°</span>\n"
"            </div>\n"
"        </div>\n"
"        \n"
"        <div class='servo-control'>\n"
"            <div class='servo-title'>舵机 4 (GPIO 10) - 范围: 0°-180°</div>\n"
"            <div class='slider-container'>\n"
"                <input type='range' min='0' max='180' value='90' class='slider' id='servo4' oninput='updateAngle(4, this.value)'>\n"
"                <span class='angle-display' id='angle4'>90°</span>\n"
"            </div>\n"
"        </div>\n"
"        \n"
"        <div class='button-group'>\n"
"            <button class='reset-btn' onclick='resetAllServos()'>重置所有舵机到中位</button>\n"
"            <button class='reset-btn' onclick='startTracking()'>启动追踪模式</button>\n"
"            <button class='reset-btn' onclick='stopTracking()'>停止追踪模式</button>\n"
"            <button class='reset-btn' onclick='scanAction()'>执行随机动作</button>\n"
"        </div>\n"
"    </div>\n"
"    \n"
"    <script>\n"
"        function updateAngle(servoId, angle) {\n"
"            document.getElementById('angle' + servoId).textContent = angle + '°';\n"
"            \n"
"            fetch('/api/servo', {\n"
"                method: 'POST',\n"
"                headers: { 'Content-Type': 'application/json' },\n"
"                body: JSON.stringify({ servo_id: servoId - 1, angle: parseInt(angle) })\n"
"            })\n"
"            .then(response => response.json())\n"
"            .then(data => {\n"
"                showStatus(data.success ? '舵机 ' + servoId + ' 设置成功' : '设置失败: ' + data.message, data.success);\n"
"            })\n"
"            .catch(error => {\n"
"                showStatus('网络错误: ' + error.message, false);\n"
"            });\n"
"        }\n"
"        \n"
"        function resetAllServos() {\n"
"            // 每个舵机的中位角度\n"
"            const centerAngles = [85, 135, 85, 90]; // 对应舵机1,2,3,4\n"
"            \n"
"            for (let i = 1; i <= 4; i++) {\n"
"                const centerAngle = centerAngles[i-1];\n"
"                document.getElementById('servo' + i).value = centerAngle;\n"
"                document.getElementById('angle' + i).textContent = centerAngle + '°';\n"
"                updateAngle(i, centerAngle);\n"
"            }\n"
"        }\n"
"        \n"
"        function startTracking() {\n"
"            fetch('/api/tracking', {\n"
"                method: 'POST',\n"
"                headers: { 'Content-Type': 'application/json' },\n"
"                body: JSON.stringify({ action: 'start' })\n"
"            })\n"
"            .then(response => response.json())\n"
"            .then(data => showStatus(data.message, data.success))\n"
"            .catch(error => showStatus('启动追踪模式失败', false));\n"
"        }\n"
"        \n"
"        function stopTracking() {\n"
"            fetch('/api/tracking', {\n"
"                method: 'POST',\n"
"                headers: { 'Content-Type': 'application/json' },\n"
"                body: JSON.stringify({ action: 'stop' })\n"
"            })\n"
"            .then(response => response.json())\n"
"            .then(data => showStatus(data.message, data.success))\n"
"            .catch(error => showStatus('停止追踪模式失败', false));\n"
"        }\n"
"        \n"
"        function scanAction() {\n"
"            fetch('/api/scan', {\n"
"                method: 'POST',\n"
"                headers: { 'Content-Type': 'application/json' },\n"
"                body: JSON.stringify({})\n"
"            })\n"
"            .then(response => response.json())\n"
"            .then(data => showStatus(data.message, data.success))\n"
"            .catch(error => showStatus('执行随机动作失败', false));\n"
"        }\n"
"        \n"
"        function showStatus(message, isSuccess) {\n"
"            const statusDiv = document.getElementById('status');\n"
"            statusDiv.textContent = message;\n"
"            statusDiv.className = 'status ' + (isSuccess ? 'success' : 'error');\n"
"            statusDiv.style.display = 'block';\n"
"            setTimeout(() => { statusDiv.style.display = 'none'; }, 3000);\n"
"        }\n"
"    </script>\n"
"</body>\n"
"</html>";

// MJPEG流处理函数
static esp_err_t jpg_stream_httpd_handler(httpd_req_t *req)
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

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }
        
        if(fb->format != PIXFORMAT_JPEG){
            bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
            if(!jpeg_converted){
                ESP_LOGE(TAG, "JPEG compression failed");
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
        ESP_LOGI(TAG, "MJPG: %uKB %ums (%.1ffps)", 
            (uint32_t)(jpg_buf_len/1024), 
            (uint32_t)frame_time, fps);
    }

    last_frame = 0;
    return res;
}

// 处理主页请求
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// 处理舵机控制API请求
static esp_err_t servo_api_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;
    
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }
    
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout");
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // 解析JSON
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *servo_id_json = cJSON_GetObjectItem(json, "servo_id");
    cJSON *angle_json = cJSON_GetObjectItem(json, "angle");
    
    if (!cJSON_IsNumber(servo_id_json) || !cJSON_IsNumber(angle_json)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing servo_id or angle");
        return ESP_FAIL;
    }
    
    int servo_id = servo_id_json->valueint;
    int angle = angle_json->valueint;
    
    cJSON_Delete(json);
    
    // 验证参数
    if (servo_id < 0 || servo_id >= SERVO_MAX || angle < 0 || angle > 180) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid servo_id or angle");
        return ESP_FAIL;
    }
    
    // 控制舵机
    esp_err_t err = servo_set_angle((servo_id_t)servo_id, angle);
    
    // 构造响应
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", err == ESP_OK);
    if (err != ESP_OK) {
        cJSON_AddStringToObject(response, "message", esp_err_to_name(err));
    }
    
    char *response_str = cJSON_Print(response);
    cJSON_Delete(response);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, HTTPD_RESP_USE_STRLEN);
    
    free(response_str);
    
    ESP_LOGI(TAG, "Servo %d set to %d degrees", servo_id, angle);
    return ESP_OK;
}

// 处理追踪控制API请求
static esp_err_t tracking_api_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;
    
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }
    
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout");
        return ESP_FAIL;
    }
    
    buf[ret] = '\0';
    
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *action_json = cJSON_GetObjectItem(json, "action");
    if (!cJSON_IsString(action_json)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing action");
        return ESP_FAIL;
    }
    
    const char *action = action_json->valuestring;
    esp_err_t err = ESP_OK;
    const char *message = "";
    
    if (strcmp(action, "start") == 0) {
        err = servo_start_tracking();
        message = (err == ESP_OK) ? "追踪模式已启动" : "启动追踪模式失败";
    } else if (strcmp(action, "stop") == 0) {
        err = servo_stop_tracking();
        message = (err == ESP_OK) ? "追踪模式已停止" : "停止追踪模式失败";
    } else {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid action");
        return ESP_FAIL;
    }
    
    cJSON_Delete(json);
    
    // 构造响应
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", err == ESP_OK);
    cJSON_AddStringToObject(response, "message", message);
    
    char *response_str = cJSON_Print(response);
    cJSON_Delete(response);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, HTTPD_RESP_USE_STRLEN);
    
    free(response_str);
    
    ESP_LOGI(TAG, "Tracking %s", action);
    return ESP_OK;
}

// 处理扫描动作API请求
static esp_err_t scan_api_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;
    
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }
    
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout");
        return ESP_FAIL;
    }
    
    // 执行随机动作
    esp_err_t err = servo_scan_action();
    const char *message = (err == ESP_OK) ? "随机动作执行成功" : "随机动作执行失败";
    
    // 构造响应
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", err == ESP_OK);
    cJSON_AddStringToObject(response, "message", message);
    
    char *response_str = cJSON_Print(response);
    cJSON_Delete(response);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, HTTPD_RESP_USE_STRLEN);
    
    free(response_str);
    
    ESP_LOGI(TAG, "Random action executed");
    return ESP_OK;
}

esp_err_t start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;
    
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // 注册URI处理器
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index_uri);
        
        httpd_uri_t servo_api_uri = {
            .uri = "/api/servo",
            .method = HTTP_POST,
            .handler = servo_api_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &servo_api_uri);
        
        httpd_uri_t tracking_api_uri = {
            .uri = "/api/tracking",
            .method = HTTP_POST,
            .handler = tracking_api_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &tracking_api_uri);
        
        httpd_uri_t scan_api_uri = {
            .uri = "/api/scan",
            .method = HTTP_POST,
            .handler = scan_api_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &scan_api_uri);
        
        httpd_uri_t stream_uri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = jpg_stream_httpd_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &stream_uri);
        
        ESP_LOGI(TAG, "Web server started successfully");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start web server");
    return ESP_FAIL;
}

esp_err_t stop_web_server(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
    return ESP_OK;
}