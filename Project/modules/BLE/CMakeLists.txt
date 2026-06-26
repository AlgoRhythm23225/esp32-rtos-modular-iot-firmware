idf_component_register(SRCS "ble_manager.c"
                            "profiles/prov_profile/prov_profile.c"
                            "profiles/ota_profile/ota_profile.c"
                            "profiles/sensor_profile/sensor_profile.c"
                    INCLUDE_DIRS "include"
                                 "profiles/prov_profile"
                                 "profiles/ota_profile"
                                 "profiles/sensor_profile"
                    REQUIRES
                        bt
                        sensors_manager
                        cjson
                        ota
                        storage
                        Wifi
)   