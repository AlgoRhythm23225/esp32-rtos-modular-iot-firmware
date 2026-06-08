idf_component_register(SRCS "ble_gatts_svc.c"
                    INCLUDE_DIRS "include"
                    REQUIRES    bt
                    PRIV_REQUIRES gatt_svc_wifi
                    )
