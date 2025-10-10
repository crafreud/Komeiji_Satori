#include "who_human_face_detection.hpp"
#include "esp_log.h"
#include "esp_camera.h"
#include "dl_image.hpp"
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "who_ai_utils.hpp"
#include "esp_camera.h"
#include "esp32_s3_szp.h"

static const char *TAG = "human_face_detection";

// static QueueHandle_t xQueueLCDFrame = NULL;  // LCD队列已注释，只使用控制台输出
static QueueHandle_t xQueueAIFrame = NULL;


static bool gEvent = true;
static bool gReturnFB = true;

// AI处理任务
static void task_process_ai(void *arg)
{
    camera_fb_t *frame = NULL;
    HumanFaceDetectMSR01 detector(0.3F, 0.3F, 10, 0.3F);
    HumanFaceDetectMNP01 detector2(0.4F, 0.3F, 10);
    // HumanFaceDetectMSR01 detector(0.2F, 0.6F, 5, 0.3F);
    // HumanFaceDetectMNP01 detector2(0.6F, 0.4F, 3);
    while (true)
    {
        if (xQueueReceive(xQueueAIFrame, &frame, portMAX_DELAY))
        {
            if (gEvent)
            {
                std::list<dl::detect::result_t> &detect_candidates = detector.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3});
                std::list<dl::detect::result_t> &detect_results = detector2.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_candidates);

                if (detect_results.size() > 0)
                {
                    // 只在检测到人脸时才绘制结果和打印信息
                    draw_detection_result((uint16_t *)frame->buf, frame->height, frame->width, detect_results);
                    print_detection_result(detect_results);
                }
            }

            // 确保帧缓冲区总是被释放，避免内存泄漏
            if (gReturnFB)
            {
                esp_camera_fb_return(frame);
            }
            else
            {
                free(frame);
            }
        }
    }
}


// LCD处理任务已注释，不再使用LCD显示
// static void task_process_lcd(void *arg)
// {
//     camera_fb_t *frame = NULL;
// 
//     while (true)
//     {
//         if (xQueueReceive(xQueueLCDFrame, &frame, portMAX_DELAY))
//         {
//             lcd_draw_bitmap(0, 0, frame->width, frame->height, (uint16_t *)frame->buf);
//             esp_camera_fb_return(frame);
//         }
//     }
// }

// 摄像头处理任务
static void task_process_camera(void *arg)
{
    while (true)
    {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame)
            xQueueSend(xQueueAIFrame, &frame, portMAX_DELAY);
        
        // 添加延时控制帧率，避免帧积压
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms延时，约10fps
    }
}

// 人脸检测 - 只使用控制台输出，不使用LCD显示
void app_camera_ai_lcd(void)
{
    // LCD队列已注释，只使用AI处理
    // xQueueLCDFrame = xQueueCreate(2, sizeof(camera_fb_t *));
    xQueueAIFrame = xQueueCreate(5, sizeof(camera_fb_t *));  // 增加队列大小到5

    ESP_LOGI(TAG, "启动人脸检测系统 - 控制台输出模式");
    
    // 摄像头任务优先级设为6，AI处理任务优先级设为4，确保摄像头任务不被阻塞
    xTaskCreatePinnedToCore(task_process_camera, "task_process_camera", 3 * 1024, NULL, 6, NULL, 1);
    // LCD任务已注释，不再使用LCD显示
    // xTaskCreatePinnedToCore(task_process_lcd, "task_process_lcd", 4 * 1024, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(task_process_ai, "task_process_ai", 4 * 1024, NULL, 4, NULL, 0);
}
