cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ENV{ESP_MATTER_PATH})
    message(FATAL_ERROR "Source ESP-IDF and ESP-Matter export.sh before building")
endif()

# One firmware image per sensor variant, see variants/README.md.
# Select with: idf.py -B build/<variant> -DDNIOT_VARIANT=<variant> build
if(NOT DNIOT_VARIANT AND DEFINED ENV{DNIOT_VARIANT})
    set(DNIOT_VARIANT "$ENV{DNIOT_VARIANT}")
endif()
set(DNIOT_VARIANT "${DNIOT_VARIANT}" CACHE STRING "Sensor variant (directory name below variants/)")
set(variant_dir "${CMAKE_CURRENT_LIST_DIR}/variants/${DNIOT_VARIANT}")
if(NOT DNIOT_VARIANT OR NOT EXISTS "${variant_dir}/variant.env")
    file(GLOB known RELATIVE "${CMAKE_CURRENT_LIST_DIR}/variants" "${CMAKE_CURRENT_LIST_DIR}/variants/*/variant.env")
    string(REPLACE "/variant.env" "" known "${known}")
    message(FATAL_ERROR "Set DNIOT_VARIANT to one of: ${known} (got '${DNIOT_VARIANT}')")
endif()
file(STRINGS "${variant_dir}/variant.env" variant_lines REGEX "^[A-Z_]+=")
foreach(line IN LISTS variant_lines)
    string(REGEX MATCH "^([A-Z_]+)=(.*)$" _ "${line}")
    set(DNIOT_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
endforeach()
foreach(required PRODUCT_NAME PROJECT_VER PROJECT_VER_NUMBER)
    if(NOT DEFINED DNIOT_${required})
        message(FATAL_ERROR "${variant_dir}/variant.env lacks ${required}")
    endif()
endforeach()

option(DNIOT_DIAGNOSTIC "Diagnostic image without automatic Light-sleep" OFF)
if(NOT DNIOT_DIAGNOSTIC AND "$ENV{DNIOT_DIAGNOSTIC}" STREQUAL "1")
    set(DNIOT_DIAGNOSTIC ON)
endif()
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;variants/${DNIOT_VARIANT}/sdkconfig.defaults")
if(DNIOT_DIAGNOSTIC)
    list(APPEND SDKCONFIG_DEFAULTS "variants/diagnostic.sdkconfig")
endif()
# Keep each variant's resolved configuration in its own build directory, so switching
# variants never reuses a stale project-level sdkconfig.
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
message(STATUS "DoNetIoT variant ${DNIOT_VARIANT}: ${DNIOT_PRODUCT_NAME} ${DNIOT_PROJECT_VER} (${DNIOT_PROJECT_VER_NUMBER}), diagnostic=${DNIOT_DIAGNOSTIC}")

set(PROJECT_VER "${DNIOT_PROJECT_VER}")
set(PROJECT_VER_NUMBER ${DNIOT_PROJECT_VER_NUMBER})
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
idf_build_set_property(COMPILE_DEFINITIONS "CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME=\"${DNIOT_PRODUCT_NAME}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING=\"ESP32-H2\"" APPEND)
idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H;-DCONFIG_OPENTHREAD_CONFIG_SRP_CLIENT_BUFFERS_MAX_SERVICES=5;-Wno-overloaded-virtual" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os;-DCONFIG_OPENTHREAD_CONFIG_SRP_CLIENT_BUFFERS_MAX_SERVICES=5" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security" APPEND)
