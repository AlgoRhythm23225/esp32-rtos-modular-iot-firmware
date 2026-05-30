idf_component_register(SRCS "driver_bno055_interface.c" "driver_bno055.c" "quat_bno.c" "ndof_bno.c" "hello_bno.c" "calib_bno.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                    PRIV_REQUIRES
                        i2c 
)

