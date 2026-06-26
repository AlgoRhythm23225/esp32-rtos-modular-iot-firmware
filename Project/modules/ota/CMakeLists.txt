idf_component_register(SRCS "ota_service.c"
                            "ota_http_client.c"
                            "ota_agent.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mbedtls
                             app_update
                             esp_https_ota
                             esp_http_client 
                             config
)