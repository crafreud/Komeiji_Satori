#include "who_human_face_detection.hpp"
#include "esp_log.h"
#include "esp_camera.h"
#include "dl_image.hpp"
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "who_ai_utils.hpp"
#include "esp_camera.h"
#include "who_ai_utils.hpp"
#include <list>

static const char *TAG = "human_face_detection";
static QueueHandle_t xQueueAIFrame = NULL;


static bool gEvent = true;
static bool gReturnFB = true;

// AI处理任务
static void task_process_ai(void *arg)
{
    camera_fb_t *frame = NULL;
    HumanFaceDetectMSR01 detector(0.3F, 0.3F, 10, 0.3F);
    HumanFaceDetectMNP01 detector2(0.4F, 0.3F, 10);

    while (true)
    {
        if (gEvent)
        {
            if (xQueueReceive(xQueueAIFrame, &frame, portMAX_DELAY))
            {
                std::list<dl::detect::result_t> &detect_candidates = detector.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3});
                std::list<dl::detect::result_t> &detect_results = detector2.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_candidates);

                if (detect_results.size() > 0)
                {
                    draw_detection_result((uint16_t *)frame->buf, frame->height, frame->width, detect_results);
                    print_detection_result(detect_results);
                }
            }

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

// 摄像头处理任务
static void task_process_camera(void *arg)
{
    while (true)
    {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame)
            xQueueSend(xQueueAIFrame, &frame, portMAX_DELAY);
    }
}

// 人脸检测
void app_camera_ai_lcd(void)
{
    xQueueAIFrame = xQueueCreate(2, sizeof(camera_fb_t *));

    xTaskCreatePinnedToCore(task_process_camera, "task_process_camera", 3 * 1024, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(task_process_ai, "task_process_ai", 4 * 1024, NULL, 5, NULL, 0);
}
