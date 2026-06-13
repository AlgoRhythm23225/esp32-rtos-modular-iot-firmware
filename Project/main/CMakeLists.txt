set(COREMQTT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../IOT_PROJECT_GROUP2/aws-iot-device-sdk-embedded-C/libraries/standard/coreMQTT")
set(COREMQTT_SRCS
    "${COREMQTT_ROOT}/source/core_mqtt.c"
    "${COREMQTT_ROOT}/source/core_mqtt_serializer.c"
    "${COREMQTT_ROOT}/source/core_mqtt_state.c"
)
set(COREMQTT_INCLUDES
    "${COREMQTT_ROOT}/source/include"
    "${COREMQTT_ROOT}/source/interface"
)
idf_component_register(
    SRCS "Project.c"
        "../modules/Wifi/wifi_task.c"
        "../modules/MQTT/mqtt_task.c"
        "../modules/BLE/ble_manager.c"
        "../modules/BLE/ble_gatts_svc.c"
        "../modules/BLE/gatt_svc_wifi.c"
        "../modules/BLE/nvs_manager.c"
        ${COREMQTT_SRCS}
    INCLUDE_DIRS 
        "."
        "../include"
        "../config"
        "../modules/BLE/include"
        "../modules/MQTT/include"
        "../modules/Wifi/include"
        ${COREMQTT_INCLUDES}
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_timer esp-tls esp_driver_gpio bt i2c_bus bno055
     EMBED_TXTFILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../certs/AmazonRootCA1.pem"
        "${CMAKE_CURRENT_SOURCE_DIR}/../certs/device.crt"
        "${CMAKE_CURRENT_SOURCE_DIR}/../certs/device.key"
)
set_source_files_properties(
    ${COREMQTT_SRCS}
    PROPERTIES COMPILE_FLAGS "-Wno-error=unused-value"
)
