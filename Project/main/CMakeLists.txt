# Quy hoạch đường dẫn AWS IoT SDK về nội bộ bên trong modules của dự án
set(COREMQTT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../modules/MQTT/coreMQTT")

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
        ${COREMQTT_SRCS}
    INCLUDE_DIRS 
        "."
        "../config"
        "../modules/BLE/include"
        "../modules/MQTT/include"
        "../modules/Wifi/include"
        ${COREMQTT_INCLUDES}
    REQUIRES 
        esp_wifi 
        esp_event 
        esp_netif 
        esp_timer 
        esp-tls 
        esp_driver_gpio 
        sensors_manager
        storage
    EMBED_TXTFILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../certs/AmazonRootCA1.pem"
        "${CMAKE_CURRENT_SOURCE_DIR}/../certs/device.crt"
        "${CMAKE_CURRENT_SOURCE_DIR}/../certs/device.key"
)

set_source_files_properties(
    ${COREMQTT_SRCS}
    PROPERTIES COMPILE_FLAGS "-Wno-error=unused-value"
)