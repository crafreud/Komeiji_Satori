/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动Web服务器
 * 
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t start_web_server(void);

/**
 * @brief 停止Web服务器
 * 
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t stop_web_server(void);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H