idf_component_register(SRCS "bmp280.c" "driver_bmp280_interface.c" "driver_bmp280.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        sensor_driver_interface_manager
                    PRIV_REQUIRES
                        i2c_bus
                        app_types
)
