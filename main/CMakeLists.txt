idf_component_register(SRCS "ipn-nodo.c" "nodo_bluetooth.c" "init.c"
    "nodo_wifi.c" "web/http.c" "web/websocket.c" "utils/time.c"
    "cJSON.c" "nodo_nvs.c" "measure.c" "web/json.c" "nodo_mac.c"
    "nodo_gattc.c" "nodo_gatts.c" "nodo_spiffs.c" "measure_def.c" "gatt_def.c"
                    INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-but-set-parameter)
