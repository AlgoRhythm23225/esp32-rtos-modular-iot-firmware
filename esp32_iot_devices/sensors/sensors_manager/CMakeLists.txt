idf_component_register(SRCS "sensors_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                    PRIV_REQUIRES
                        i2c
                        bh1750
                        bme280
                        bmp280
                        aht20    
                        ina226
                        bno055  
)
