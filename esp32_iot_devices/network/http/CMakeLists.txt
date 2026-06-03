idf_component_register(SRCS "http_client.c" "api.c" "core_http_client.c" "http.c" "llhttp.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        esp_driver_i2c 
                        lwip
                    PRIV_REQUIRES
                        i2c
                        bh1750
                        bme280
                        bmp280                  
)
