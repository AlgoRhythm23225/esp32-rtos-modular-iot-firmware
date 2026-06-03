idf_component_register(SRCS "socket_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        lwip
                    PRIV_REQUIRES                 
)
