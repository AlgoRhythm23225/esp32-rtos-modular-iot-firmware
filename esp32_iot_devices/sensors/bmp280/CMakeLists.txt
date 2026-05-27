idf_component_register(SRCS "bmp280.c" "driver_bmp280_interface.c" "driver_bmp280.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                    PRIV_REQUIRES
                        i2c 
)
