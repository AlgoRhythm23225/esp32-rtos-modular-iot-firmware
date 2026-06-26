idf_component_register(SRCS "aht20.c" "driver_aht20_interface.c" "driver_aht20.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        sensor_driver_interface_manager
                    PRIV_REQUIRES
                        i2c_bus
)
