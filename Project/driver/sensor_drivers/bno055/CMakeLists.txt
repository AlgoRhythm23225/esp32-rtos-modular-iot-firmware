idf_component_register(SRCS "bno055.c" "driver_bno055_interface.c" "driver_bno055.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                    PRIV_REQUIRES
                        i2c_bus
                        main
                        sensor_driver_interface_manager
)

