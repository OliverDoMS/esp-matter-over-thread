idf_component_register(SRCS "app_main.cpp" "board.cpp" "sensors.cpp" "soil_moisture.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES esp_adc)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
