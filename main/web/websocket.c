#include "web/websocket.h"

/* GATTC Init */
static uint16_t const init_char[] = { TEST_DISCOVERY_UUID };

static void ws_evt_handler_conn(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void ws_evt_handler_data(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void ws_message_handler(cJSON *msg, ws_msg_handler_arg_t *args);

void websocket_task(void *pvParameters) {
    static char headers[HEADER_BUFFER_LEN];
    static ws_msg_handler_arg_t handler_arg;
    ws_queue_msg_t msg = {0};
    ws_task_arg_t *arg;
    arg = (ws_task_arg_t *) pvParameters;
    handler_arg.ctrl_queue = arg->ctrl_queue;
    handler_arg.out_queue = arg->out_queue;
    // Esperar a que haya conexión WiFI
    ESP_LOGI(WEBSOCK_TAG, "Esperando conexión WiFi...");
    xEventGroupWaitBits(arg->nodo_evt_group, COMM_CHANNEL_OK, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(WEBSOCK_TAG, "Preparando WebSocket...");
    /* Acomodar header para token */
    sprintf(headers, "Authorization: %s\r\n", arg->token);
    /*             Importante!   ~~~~~^^^^*/
    /* Configurar cliente WebSocket */
    esp_websocket_client_config_t websocket_cfg = {
        .user_agent = NODO_USER_AGENT,
        .host = SERVER_URL,
        .port = WEBSOCKET_PORT,
        .transport = WEBSOCKET_TRANSPORT_OVER_TCP,
        .ping_interval_sec = WS_PING_INTERVAL,
        .headers = headers,
    };
    esp_websocket_client_handle_t ws_client = esp_websocket_client_init(&websocket_cfg);
    /* Registrar handler para WEBSOCKET_EVENT_CONNECTED/DISCONNECTED */
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_CONNECTED, ws_evt_handler_conn, (void *) &(arg->nodo_evt_group));
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_DISCONNECTED, ws_evt_handler_conn, (void *) &(arg->nodo_evt_group));
    /* Registrar handler para WEBSOCKET_EVENT_DATA/ERROR */
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_DATA, ws_evt_handler_data, &handler_arg);
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ERROR, ws_evt_handler_data, NULL);
    esp_websocket_client_start(ws_client);
    while (esp_websocket_client_is_connected(ws_client) != true) {
        ESP_LOGW(WEBSOCK_TAG, "Esperando conexion con WebSocket..."); 
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    char buffer[JSON_BUFFER_SIZE];
    for(;;) {
        ESP_LOGI(WEBSOCK_TAG, "Esperando mensajes..");
        xQueueReceive(arg->out_queue, (void *) &msg, portMAX_DELAY);
        memset(buffer, 0, JSON_BUFFER_SIZE);
        if (msg.type == MSG_MEAS_VECTOR_INST || MSG_MEAS_VECTOR_NORM) {
            ESP_LOGI(WEBSOCK_TAG, "MSG_MEAS_VECTOR");
            cJSON *mvector_json = get_measure_vector_json(msg.meas_vector);
            int ret = json_wrap_message_buff(STATUS_OK, 
                    (msg.type == MSG_MEAS_VECTOR_INST) ? LIVE_DATA : SAVE_DATA,
                     mvector_json, buffer, 
                     JSON_BUFFER_SIZE );
            if ( ret != 0 ) {
                ESP_LOGE(WEBSOCK_TAG, "%s| JSON buffer agotado!", __func__);
                continue;
            }
            ESP_LOGI(WEBSOCK_TAG, "Enviando mensaje al servidor...");
            //ESP_LOGI(WEBSOCK_TAG, "JSON:\n%s", buffer);
            //ESP_LOG_BUFFER_HEX(WEBSOCK_TAG, json_str, strlen(json_str));
            esp_websocket_client_send_text(ws_client, buffer, strlen(buffer), portMAX_DELAY);
        } else if (msg.type == MSG_STATUS_GENERIC) {
            ESP_LOGI(WEBSOCK_TAG, "MSG_STATUS_GENERIC");
            cJSON *msg_generic_json = get_generic_msg_json(msg.status.esp_status, msg.status.status);
            if ( msg_generic_json != NULL && 
                    cJSON_PrintPreallocated(msg_generic_json, buffer, JSON_BUFFER_SIZE, false) == 0 ) {
                ESP_LOGE(WEBSOCK_TAG, "json_wrap_message_buff: Error guardando JSON en buffer!");
            }
            cJSON_Delete(msg_generic_json);
            esp_websocket_client_send_text(ws_client, buffer, strlen(buffer), portMAX_DELAY);
        }
    }
    vTaskDelete(NULL);
}

/* Websocket handler para conexión/desconexión */
static void ws_evt_handler_conn(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    EventGroupHandle_t *evt_group = (EventGroupHandle_t *) handler_args;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(WEBSOCK_TAG, "WEBSOCKET_EVENT_CONNECTED");
            xEventGroupSetBits(*evt_group, COMM_DISPATCHER_OK);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(WEBSOCK_TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            xEventGroupClearBits(*evt_group, COMM_DISPATCHER_OK);
            // TODO: Reactivar conexión
            break;
    }
}

/* Websocket handler para intercambio de paquetes de datos */
static void ws_evt_handler_data(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_DATA:
            ESP_LOGI(WEBSOCK_TAG, "WEBSOCKET_EVENT_DATA");
            ESP_LOGI(WEBSOCK_TAG, "Received opcode=%d", data->op_code);
            if (data->op_code == 0x08 && data->data_len == 2) {
                ESP_LOGI(WEBSOCK_TAG, "%s: Received closed message with code=%d"
                        , __func__ ,256*data->data_ptr[0] + data->data_ptr[1]);
            } else if (data->op_code == 0xa) {
                ESP_LOGI(WEBSOCK_TAG, "%s: Pong", __func__);
            } else {
                ESP_LOGI(WEBSOCK_TAG, "Received=%.*s", data->data_len, (char *)data->data_ptr);
                cJSON *msg_json = cJSON_ParseWithLength(data->data_ptr, data->data_len);
                if (msg_json == NULL) {
                    ESP_LOGE(WEBSOCK_TAG, "%s Error parsing message", __func__);
                    break;
                }
                ws_message_handler(msg_json, (ws_msg_handler_arg_t *) handler_args );
                cJSON_Delete(msg_json); 
            }
            //ESP_LOGI(WEBSOCK_TAG, "Total payload length=%d, data_len=%d, current payload offset=%d", data->payload_len, data->data_len, data->payload_offset);
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGI(WEBSOCK_TAG, "WEBSOCKET_EVENT_ERROR");
            break;
    }
}

/*
 * Función para procesamiento de mensajes del servidor (que no son tramas de
 * control).
 *
 * Argumentos:
 *      msg : Mensaje en formato JSON
 *      ws_queue : Handle para la cola de mensajes del WebSocket. Usado para
 *      envíar respuesta a mensajes que lo necesiten.
 */
static void ws_message_handler(cJSON *msg, ws_msg_handler_arg_t *args) {
    ESP_LOGI(WEBSOCK_TAG, "%s", __func__);
    cJSON *msg_type = cJSON_GetObjectItem(msg, "type");
    if ( !cJSON_IsString(msg_type) && msg_type->valuestring == NULL ) {
        ESP_LOGE(WEBSOCK_TAG, "%s: Error parsing type", __func__);
        return; 
    }
    cJSON *content = cJSON_GetObjectItem(msg, "content");
    if (!cJSON_IsObject(content)) {
        ESP_LOGE(WEBSOCK_TAG, "%s: Error parsing content", __func__);
        return; 
    }
    if ( strcmp(msg_type->valuestring, "req-inst-data") == 0 ) {
        /* Preparar el argumento para el callback */
        cJSON *child = cJSON_GetObjectItem(content, "devAddr");
        if ( child == NULL ) {
            ESP_LOGE(WEBSOCK_TAG, "%s| Error parsing req-inst-data!", __func__);
            return;
        }
        ctrl_msg_t ctrl_msg = {.type = MSG_MEASURE_INST, .remote_addr = NULL};  
        /* Notificar al control */
        xQueueSendToBack(args->ctrl_queue,&ctrl_msg, portMAX_DELAY);
    }
    /* TODO: Refactorizar a la parte de control en measure.c */
    if ( strcmp(msg_type->valuestring, "esp-dev-discovery") == 0 ) {
        uint8_t *addr;
        /* Preparar el argumento para el callback */
        cJSON *child = cJSON_GetObjectItem(content, "child");

        if ( ( addr = parse_u8_array(child, ENTRY_LEN) ) == NULL ) {
            return;
        }
        ESP_LOG_BUFFER_HEX(WEBSOCK_TAG, addr, ENTRY_LEN);
        /* Habilitar el stack BT/BLE sí no esta habilitado */
        if (dev_type == NODO_WIFI) {
            //ESP_LOGI(WEBSOCK_TAG, "%s| Debería habilitar BT/BLE & iniciar GATTC...", __func__);
            ESP_LOGI(WEBSOCK_TAG, "%s: Habilitando stack BT/BLE...", __func__);
            if ( nodo_bt_init(ESP_BT_MODE_BLE) != 0 ) {
                ESP_LOGE(WEBSOCK_TAG, "%s: Error iniciando stack Bluetooth!", __func__);
                return;
            }
            if (init_gatt_client(args->out_queue) != 0) {
                ESP_LOGE(WEBSOCK_TAG, "%s: Error levantando cliente GATT!", __func__); 
                return;
            }
            register_discovery_cb(gattc_ws_callback);
            // TODO: Agregar str_addr para DISCOVERY_CMPL/FAIL
            char *addr_str = (char *) calloc(MAC_STR_LEN + 1, sizeof(char));
            get_mac_str(addr, addr_str);
            gattc_set_addr(addr, addr_str);
            if ( gattc_set_chars(init_char, sizeof(init_char)/sizeof(init_char[0])) != 0) {
                ESP_LOGE(WEBSOCK_TAG, "%s: Error declarando chars!", __func__); 
                return; 
            }
            for ( uint8_t i = 0; i < MAX_GATTC_ATTEMPTS; i++ ) {
                int ret = nodo_gattc_start();
                if ( ret != 0 ) {
                    ESP_LOGE(MEAS_TAG, "%s GATTC reintento #%02d", __func__, i);
                    vTaskDelay(pdMS_TO_TICKS(GATTC_WAIT_START_TIMEOUT));
                } else {
                    break;
                }
            }
            free(addr_str);
        }
        ESP_LOGI(WEBSOCK_TAG, "%s| Done!", __func__);
        // Discovery...
    } else {
        ESP_LOGW(WEBSOCK_TAG, "Tipo de msg. no reconocido");
    }
}

/* Callback enviado al proceso de inicialización de BLE */
void gattc_ws_callback(nodo_gattc_events_t evt, void *arg) {
    /* TODO: Emitir mensaje donde falla el proceso de descubrimiento
     * |-> Parcialmente cubierto en nodo_gattc.h! */
    if ( evt == DISCOVERY_CMPL ) {
        /* Guardar en SPIFFS */
        if ( ( dev_type == NODO_WIFI ) &&  ( spiffs_init() != 0 ) ) {
            ESP_LOGE(WEBSOCK_TAG, "%s| Error inicializando almacenamiento SPIFFS!", __func__);
            return;
        }
        if ( ( spiffs_mounted() != 0 ) && ( spiffs_mount() != 0 ) ) {
            ESP_LOGE(WEBSOCK_TAG, "%s| Error montando SPIFFS!", __func__);
            return;
        }
        if (spiffs_add_entry( (uint8_t *) arg ) != 0 ) {
            ESP_LOGE(WEBSOCK_TAG, "%s| Error guardando entrada!", __func__);
            return;
        }
        if ( ( dev_type == NODO_WIFI ) && ( nvs_set_mode(SINKNODE) != 0 ) ) {
            ESP_LOGE(WEBSOCK_TAG, "%s| Error guardando nuevo modo (SINKNODE)!", __func__);
        }
        spiffs_umount();
        vTaskDelay(pdMS_TO_TICKS(3000));
        /* Reiniciar para inicializar con nuevo modo o leer de nuevo la lista y
         * realizar la primera lectura */
        esp_restart();
        /* Ruta alternativa sin reinicio <<< Genera errores de ejecución >>> */
        //gattc_db.len += 1;
        //db_entry_t *ret = reallocarray(gattc_db.data, gattc_db.len, sizeof(db_entry_t));
        //if ( ret == NULL )
        //    ESP_LOGE(WEBSOCK_TAG, "%s| Error modificando gattc_db!", __func__);
        //else
        //    gattc_db.data = ret;
        ///* Rellenar datos de la última estación agregada */
        //uint8_t *tmp = (uint8_t *) calloc(MAC_BYTES, sizeof(uint8_t));
        //if ( tmp != NULL ) {
        //    memcpy(tmp, arg, MAC_BYTES);
        //} else {
        //    ESP_LOGE(WEBSOCK_TAG, "%s| Error alojando memoria para raw_addr!", __func__);
        //    return;
        //}
        //gattc_db.data[gattc_db.len - 1].raw_addr = tmp;
        //char *tmp_str = (char *) calloc(MAC_STR_LEN + 1, sizeof(char));
        //get_mac_str(gattc_db.data[gattc_db.len - 1].raw_addr, tmp_str);
        //gattc_db.data[gattc_db.len - 1].str_addr = tmp_str;
        ///* Establecer los parámetros de lectura para GATTC */
        //if ( gattc_set_chars(chars, sizeof(chars)/sizeof(chars[0])) != 0) {
        //    ESP_LOGE(WEBSOCK_TAG, "%s: Error declarando chars!", __func__); 
        //    return; 
        //}
        unregister_discovery_cb();
    }
}
