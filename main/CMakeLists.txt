idf_component_register(SRCS "ipn-nodo.c" "nodo_bluetooth.c" "init.c" "nodo_wifi.c" "nodo_web.c"
    "cJSON.c" "nodo_nvs.c" "measure.c" "nodo_json.c" "nodo_mac.c"
                    INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-but-set-parameter)
