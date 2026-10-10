#include <stdio.h>
#include "esp32_s3_szp.h"
#include "sensor.h"  // 包含摄像头传感器定义


static const char *TAG = "esp32_s3_szp";

/******************************************************************************/
/***************************  I2C ↓ *******************************************/
// 使用新的driver/i2c_master.h API初始化I2C总线
static i2c_master_bus_handle_t i2c_bus_handle = NULL;

esp_err_t bsp_i2c_init(void)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = BSP_I2C_NUM,
        .scl_io_num = BSP_I2C_SCL,
        .sda_io_num = BSP_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C master bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "I2C master bus initialized successfully");
    return ESP_OK;
}
/***************************  I2C ↑  *******************************************/
/*******************************************************************************/


/*******************************************************************************/
/***************************  姿态传感器 QMI8658 ↓   ****************************/
// QMI8658传感器相关函数已移除（当前未使用，避免I2C驱动冲突）
/***************************  姿态传感器 QMI8658 ↑  ***************************/
/*******************************************************************************/


/***********************************************************/
/***************    IO扩展芯片 ↓   *************************/
// PCA9557相关函数 - 使用新的I2C驱动API (driver/i2c_master.h)

static i2c_master_dev_handle_t pca9557_dev_handle = NULL;

// 初始化PCA9557设备句柄
esp_err_t pca9557_device_init(void)
{
    if (i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t pca9557_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCA9557_SENSOR_ADDR,
        .scl_speed_hz = BSP_I2C_FREQ_HZ,
    };

    return i2c_master_bus_add_device(i2c_bus_handle, &pca9557_cfg, &pca9557_dev_handle);
}

// 读取PCA9557寄存器的值
esp_err_t pca9557_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (pca9557_dev_handle == NULL) {
        esp_err_t ret = pca9557_device_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return i2c_master_transmit_receive(pca9557_dev_handle, &reg_addr, 1, data, len, 1000);
}

// 给PCA9557的寄存器写值
esp_err_t pca9557_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    if (pca9557_dev_handle == NULL) {
        esp_err_t ret = pca9557_device_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(pca9557_dev_handle, write_buf, sizeof(write_buf), 1000);
}

// 初始化PCA9557 IO扩展芯片
void pca9557_init(void)
{
    // 写入控制引脚默认值 DVP_PWDN=1  PA_EN = 0  LCD_CS = 1
    pca9557_register_write_byte(PCA9557_OUTPUT_PORT, 0x05);  
    // 把PCA9557芯片的IO1 IO1 IO2设置为输出 其它引脚保持默认的输入
    pca9557_register_write_byte(PCA9557_CONFIGURATION_PORT, 0xf8); 
}

// 设置PCA9557芯片的某个IO引脚输出高低电平
esp_err_t pca9557_set_output_state(uint8_t gpio_bit, uint8_t level)
{
    uint8_t data;
    esp_err_t res = ESP_FAIL;

    pca9557_register_read(PCA9557_OUTPUT_PORT, &data, 1);
    res = pca9557_register_write_byte(PCA9557_OUTPUT_PORT, SET_BITS(data, gpio_bit, level));

    return res;
}

// 控制 PCA9557_LCD_CS 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void lcd_cs(uint8_t level)
{
    pca9557_set_output_state(LCD_CS_GPIO, level);
}

// 控制 PCA9557_PA_EN 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void pa_en(uint8_t level)
{
    pca9557_set_output_state(PA_EN_GPIO, level);
}

// 控制 PCA9557_DVP_PWDN 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void dvp_pwdn(uint8_t level)
{
    pca9557_set_output_state(DVP_PWDN_GPIO, level);
}



/***************    IO扩展芯片 ↑   *************************/
/***********************************************************/


/***********************************************************/
/****************    LCD显示屏 ↓   *************************/
// LCD相关函数已移除，避免与摄像头引脚冲突
/***************    LCD显示屏 ↑   *************************/
/***********************************************************/



/***********************************************************/
/****************    摄像头 ↓   ****************************/

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
    .xclk_freq_hz = 20000000,  // 提高时钟频率以改善帧率
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_RGB565,  // 使用RGB565格式，与ESP-DL人脸检测模型兼容
    .frame_size = FRAMESIZE_QVGA,  // 320x240 - 适中分辨率
    .jpeg_quality = 10,  // JPEG质量设置（RGB565模式下不使用）
    .fb_count = 2,  // 双缓冲提高稳定性
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,  // 当缓冲区空时获取帧
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


/********************    摄像头 ↑   *************************/
/***********************************************************/