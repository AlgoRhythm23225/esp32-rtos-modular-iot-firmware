idf_component_register(SRCS "ina226.c" "driver_ina226_interface.c" "driver_ina226.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        sensor_driver_interface_manager
                    PRIV_REQUIRES
                        i2c_bus
)
