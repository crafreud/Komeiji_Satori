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
#include <string.h>

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

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
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div class='container'>\n"
"        <h1>🎛️ 舵机控制面板</h1>\n"
"        <div id='status' class='status' style='display:none;'></div>\n"
"        \n"
"        <div class='servo-control'>\n"
"            <div class='servo-title'>舵机 1 (GPIO 1)</div>\n"
"            <div class='slider-container'>\n"
"                <input type='range' min='0' max='180' value='90' class='slider' id='servo1' oninput='updateAngle(1, this.value)'>\n"
"                <span class='angle-display' id='angle1'>90°</span>\n"
"            </div>\n"
"        </div>\n"
"        \n"
"        <div class='servo-control'>\n"
"            <div class='servo-title'>舵机 2 (GPIO 2)</div>\n"
"            <div class='slider-container'>\n"
"                <input type='range' min='0' max='180' value='90' class='slider' id='servo2' oninput='updateAngle(2, this.value)'>\n"
"                <span class='angle-display' id='angle2'>90°</span>\n"
"            </div>\n"
"        </div>\n"
"        \n"
"        <div class='servo-control'>\n"
"            <div class='servo-title'>舵机 3 (GPIO 4)</div>\n"
"            <div class='slider-container'>\n"
"                <input type='range' min='0' max='180' value='90' class='slider' id='servo3' oninput='updateAngle(3, this.value)'>\n"
"                <span class='angle-display' id='angle3'>90°</span>\n"
"            </div>\n"
"        </div>\n"
"        \n"
"        <div class='servo-control'>\n"
"            <div class='servo-title'>舵机 4 (GPIO 10)</div>\n"
"            <div class='slider-container'>\n"
"                <input type='range' min='0' max='180' value='90' class='slider' id='servo4' oninput='updateAngle(4, this.value)'>\n"
"                <span class='angle-display' id='angle4'>90°</span>\n"
"            </div>\n"
"        </div>\n"
"        \n"
"        <div class='button-group'>\n"
"            <button class='reset-btn' onclick='resetAllServos()'>重置所有舵机到90°</button>\n"
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
"            for (let i = 1; i <= 4; i++) {\n"
"                document.getElementById('servo' + i).value = 90;\n"
"                document.getElementById('angle' + i).textContent = '90°';\n"
"                updateAngle(i, 90);\n"
"            }\n"
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