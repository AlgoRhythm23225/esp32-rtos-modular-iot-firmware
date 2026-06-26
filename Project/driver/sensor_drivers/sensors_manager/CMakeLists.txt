idf_component_register(SRCS "sensors_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        i2c_bus
                        aht20    
                        bh1750
                        bme280
                        bmp280
                        bno055 
                        ina226
                        config 
                    PRIV_REQUIRES
)
