idf_component_register(SRCS "ipn-nodo.c" "nodo_bluetooth.c" "init.c" "nodo_wifi.c"
    "web/http.c" "web/json.c" "web/websocket.c" "utils/mac.c" "utils/time.c"
    "cJSON.c" "storage/nodo_nvs.c" "measure/measure.c" "gatt/gattc.c" "gatt/gatts.c" 
    "gatt/gatt_def.c" "storage/spiffs.c" "measure/measure_def.c" INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-but-set-parameter)
