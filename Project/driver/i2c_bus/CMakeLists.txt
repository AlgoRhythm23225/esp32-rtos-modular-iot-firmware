idf_component_register(SRCS "i2c_scan.c" "i2c_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES
                        esp_driver_i2c 
                        app_errors  
                        app_types                                           
)
