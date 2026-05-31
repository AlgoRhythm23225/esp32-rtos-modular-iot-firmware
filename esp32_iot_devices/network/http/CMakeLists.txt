idf_component_register(SRCS "api.c" "core_http_client.c" "http.c" "llhttp.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        esp_driver_i2c   
                    PRIV_REQUIRES
                        i2c
                        bh1750
                        bme280
                        bmp280                  
)
