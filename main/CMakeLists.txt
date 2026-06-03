idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        esp_driver_i2c 
                    PRIV_REQUIRES
                        i2c
                        bh1750
                        bme280
                        bmp280
                        wifi
                        http                  
)
