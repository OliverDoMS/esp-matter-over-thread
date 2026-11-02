idf_component_register(SRCS "app_main.cpp" "board.cpp" "sensors.cpp" "device_identity.cpp"
                       INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
