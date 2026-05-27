idf_component_register(SRCS "bh1750.c" "driver_bh1750fvi_interface.c" "driver_bh1750fvi.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                    PRIV_REQUIRES
                        i2c
)
