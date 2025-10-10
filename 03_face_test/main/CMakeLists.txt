idf_component_register(SRCS "who_human_face_detection.cpp" "who_ai_utils.cpp" "esp32_s3_szp.c" "main.cpp" "web_server.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_http_server esp_wifi nvs_flash esp_netif esp_event esp_timer esp-dl)
