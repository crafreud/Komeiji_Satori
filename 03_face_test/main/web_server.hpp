#ifndef WEB_SERVER_HPP
#define WEB_SERVER_HPP

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动Web服务器
 * 
 * @return esp_err_t ESP_OK表示成功，其他值表示失败
 */
esp_err_t start_webserver(void);

/**
 * @brief 主页处理函数
 * 
 * @param req HTTP请求
 * @return esp_err_t 处理结果
 */
esp_err_t index_handler(httpd_req_t *req);

/**
 * @brief JPEG图像捕获处理函数
 * 
 * @param req HTTP请求
 * @return esp_err_t 处理结果
 */
esp_err_t capture_handler(httpd_req_t *req);

/**
 * @brief MJPEG流处理函数
 * 
 * @param req HTTP请求
 * @return esp_err_t 处理结果
 */
esp_err_t stream_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_HPP