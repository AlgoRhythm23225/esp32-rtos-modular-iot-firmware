idf_component_register(SRCS "sensor_driver_interface_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES
                        i2c_bus
                        app_types
                    PRIV_REQUIRES
)
