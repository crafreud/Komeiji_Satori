#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

// WiFi配置
#define WIFI_SSID "ESP32_Camera"
#define WIFI_PASS "12345678"
#define WIFI_CHANNEL 1
#define MAX_STA_CONN 4

// HTTP服务器配置
#define SERVER_PORT 80

/**
 * @brief 初始化WiFi热点
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t wifi_init_softap(void);

/**
 * @brief 启动HTTP服务器
 * @return httpd_handle_t 服务器句柄，NULL表示失败
 */
httpd_handle_t start_webserver(void);

/**
 * @brief 停止HTTP服务器
 * @param server 服务器句柄
 */
void stop_webserver(httpd_handle_t server);

/**
 * @brief 获取摄像头图像处理程序
 * @param req HTTP请求
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t camera_handler(httpd_req_t *req);

/**
 * @brief 获取主页处理程序
 * @param req HTTP请求
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t index_handler(httpd_req_t *req);

/**
 * @brief MJPEG视频流处理程序
 * @param req HTTP请求
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t stream_handler(httpd_req_t *req);

#endif // WIFI_SERVER_H