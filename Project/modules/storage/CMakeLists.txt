idf_component_register(SRCS "storage_service.c"
                    INCLUDE_DIRS "include"
                    REQUIRES 
                        nvs_flash 
                        mbedtls
)
