idf_component_register(SRCS "gatt_svc_wifi.c" 
                    INCLUDE_DIRS "include"
                    REQUIRES bt
                    PRIV_REQUIRES nvs_manager
                    )
