idf_component_register(SRCS "main.c" "cv_motion_detection.cpp" "servo_control.c" "web_server.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_timer driver esp_http_server esp_wifi nvs_flash json)
