idf_component_register(SRCS "bme280.c" "driver_bme280_interface.c" "driver_bme280.c" 
                    INCLUDE_DIRS "."
                    REQUIRES
                        sensor_driver_interface_manager                  
                    PRIV_REQUIRES
                        i2c_bus
)
