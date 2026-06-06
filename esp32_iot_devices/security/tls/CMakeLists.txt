idf_component_register(SRCS "tls_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        esp-tls
                    PRIV_REQUIRES
)
