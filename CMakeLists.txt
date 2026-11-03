cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ENV{ESP_MATTER_PATH})
    message(FATAL_ERROR "Source ESP-IDF and ESP-Matter export.sh before building")
endif()

set(PROJECT_VER "0.4.0-soil-ota")
set(PROJECT_VER_NUMBER 10)
set(ESP_MATTER_PATH $ENV{ESP_MATTER_PATH})
set(MATTER_SDK_PATH "${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS
    "${ESP_MATTER_PATH}/examples/common"
    "${MATTER_SDK_PATH}/config/esp32/components"
    "${ESP_MATTER_PATH}/components")

project(esp_matter_base)
# Apply to the Matter SDK's device-info provider as well as the application.
idf_build_set_property(COMPILE_DEFINITIONS "CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME=\"DoNetIoT\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME=\"MoistureSensor\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING=\"ESP32-H2\"" APPEND)
idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H;-DCONFIG_OPENTHREAD_CONFIG_SRP_CLIENT_BUFFERS_MAX_SERVICES=5;-Wno-overloaded-virtual" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os;-DCONFIG_OPENTHREAD_CONFIG_SRP_CLIENT_BUFFERS_MAX_SERVICES=5" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security" APPEND)
