idf_component_register(SRCS "wifi_task.c"
                    INCLUDE_DIRS "include"
                    REQUIRES 
                        esp_wifi
                        config
                        esp_timer
)
