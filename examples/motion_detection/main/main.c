/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "esp_camera.h"
#include "servo_control.h"
#include "web_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#define CONFIG_IDF_TARGET_ARCH_XTENSA 1

#define WIFI_SSID "ESP32_Servo_Control"
#define WIFI_PASS "12345678"
#define WIFI_CHANNEL 1
#define MAX_STA_CONN 4

static const char *TAG = "MAIN";
static esp_netif_t *wifi_netif = NULL;

#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    17
#define CAM_PIN_SIOD    20
#define CAM_PIN_SIOC    19
#define CAM_PIN_D7      42
#define CAM_PIN_D6      41
#define CAM_PIN_D5      40
#define CAM_PIN_D4      39
#define CAM_PIN_D3      38
#define CAM_PIN_D2      13
#define CAM_PIN_D1      12
#define CAM_PIN_D0      11
#define CAM_PIN_VSYNC   8
#define CAM_PIN_HREF    18
#define CAM_PIN_PCLK    16

#ifdef CONFIG_IDF_TARGET_ARCH_XTENSA

static camera_config_t camera_config = {
    .pin_pwdn  = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

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

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 10000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_GRAYSCALE,//YUV422,PIXFORMAT_GRAYSCALE,RGB565,PIXFORMAT_JPEG
    .frame_size = FRAMESIZE_QVGA,//QQVGA-QXGA Do not use sizes above QVGA when not JPEG
    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1, //if more than one, i2s runs in continuous mode. Use only with JPEG

    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,

};

#endif // CONFIG_IDF_TARGET_ARCH_XTENSA


int cv_motion_detection(uint8_t* data, int width, int heigth);

// WiFi事件处理函数
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x joined, AID=%d",
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5], event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x left, AID=%d",
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5], event->aid);
    }
}

// 初始化WiFi AP模式
static esp_err_t wifi_init_ap(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

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

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(wifi_netif, &ip_info);
    ESP_LOGI(TAG, "WiFi AP started. SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
    ESP_LOGI(TAG, "Connect to http://" IPSTR, IP2STR(&ip_info.ip));

    return ESP_OK;
}

#ifdef CONFIG_IDF_TARGET_ARCH_XTENSA
esp_err_t camera_capture()
{
    //acquire a frame
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE("CAM", "Camera Capture Failed");
        return ESP_FAIL;
    }

    // Test text area detection
    cv_motion_detection(fb->buf, fb->width, fb->height);  

    esp_camera_fb_return(fb);
    return ESP_OK;
}
#endif // CONFIG_IDF_TARGET_ARCH_XTENSA

void cv_print_info();

void app_main(void)
{
    printf("Init\n");
    cv_print_info();
    
    // 初始化WiFi AP模式
    esp_err_t wifi_ret = wifi_init_ap();
    if (wifi_ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(wifi_ret));
        return;
    }
    
    // 初始化舵机控制系统
    esp_err_t servo_ret = servo_control_init();
    if (servo_ret != ESP_OK) {
        ESP_LOGE(TAG, "Servo control init failed: %s", esp_err_to_name(servo_ret));
        return;
    }
    ESP_LOGI(TAG, "Servo control system initialized");
    
    // 启动追踪模式
    esp_err_t tracking_ret = servo_start_tracking();
    if (tracking_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start tracking mode: %s", esp_err_to_name(tracking_ret));
        return;
    }
    ESP_LOGI(TAG, "Tracking mode started");
    
    // 启动Web服务器
    esp_err_t web_ret = start_web_server();
    if (web_ret != ESP_OK) {
        ESP_LOGE(TAG, "Web server start failed: %s", esp_err_to_name(web_ret));
        return;
    }
    ESP_LOGI(TAG, "Web server started");

#ifdef CONFIG_IDF_TARGET_ARCH_XTENSA
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE("CAM", "Camera Init Failed");
        return;
    }
    sensor_t *s = esp_camera_sensor_get();
    ESP_LOGI("CAM", "Camera sensor %2.2x %2.2x %4.4x %2.2x", s->id.MIDH, s->id.MIDL, s->id.PID, s->id.VER);

    //initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID)
    {
        s->set_brightness(s, 1);  //up the brightness just a bit
        s->set_saturation(s, -2); //lower the saturation
    }

    while(1)
    {
        int64_t fr_start = esp_timer_get_time();

        camera_capture();
        
        int64_t fr_end = esp_timer_get_time();
        float fps = 1*1000000/(fr_end - fr_start);
        ESP_LOGW("OpenCV", "Motion detection - %2.2f FPS", fps);
    }
#else
    ESP_LOGW("OpenCV", "Camera module not defined!");
#endif
}
