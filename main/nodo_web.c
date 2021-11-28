#include "nodo_web.h"

#define MAX_SEND_ATTEMPTS       6    // Numero máximo de intentos para intercambiar el token temporal

esp_err_t nodo_http_init_handler(esp_http_client_event_t *evt);
static char *response_body = NULL;
static int response_len = 0;
/** Funciones locales **/
static void ws_evt_handler_conn(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void ws_evt_handler_data(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
cJSON *get_measure_vector_json(measure_vector_t *mvector);
int json_wrap_message_buff(enum json_msg_status_t , enum json_msg_type_t ,  cJSON *, char *, size_t);
char *get_timestamp();
void ws_message_handler(cJSON *msg, QueueHandle_t *ws_queue);
cJSON *get_generic_msg_json(esp_err_t esp_status, uint8_t status);

extern dev_type_t dev_type;
extern spiffs_db_t gattc_db;

//const static uint16_t chars[3] = { TEMP_CHAR_UUID, HUMIDITY_CHAR_UUID, LUX_CHAR_UUID};

/*
 * Configuración del cliente HTTP para enviar el token
 */
esp_http_client_config_t config = {
    .host = HTTP_HOST,
    .path = INIT_PATH,
    .event_handler = nodo_http_init_handler,
    .user_agent = NODO_USER_AGENT,
    .transport_type = HTTP_TRANSPORT_OVER_TCP,
#ifdef HTTP_PORT
    .port = HTTP_PORT
#endif
};

/*
 * Enviar el token temporal mandado por Bluetooth al servidor y esperar a recibir el token de
 * conexión de verdad, enviado por HTTPS
 */
token_ret_t http_send_token(uint8_t *token, const char *mac) {
    esp_err_t err;
    token_ret_t ret = {0};
    ESP_LOGD(WEB_TAG, "%s: Enviando Token : %s, MAC: %s", __func__, token, mac);
    /* Crear cadena JSON */
    // TODO: Cambiar al nuevo formato gral
    cJSON *json_obj = cJSON_CreateObject();
    if ( json_obj == NULL ) {
        ESP_LOGE(WEB_TAG, "%s| Error creando objeto JSON", __func__);
        ret.esp_status = ESP_FAIL;
        return ret;
    }
    cJSON *token_val = cJSON_CreateString((char *) token);
    if ( token_val == NULL ) {
        ESP_LOGE(WEB_TAG, "%s| Error cadena token_val JSON", __func__);
        ret.esp_status = ESP_FAIL;
        return ret;
    }
    cJSON_AddItemToObject(json_obj, "token", token_val);
    //cJSON *mac_val = cJSON_CreateString(mac);
    //cJSON_AddItemToObject(json_obj, "mac", mac_val);
    char *post_data = cJSON_PrintUnformatted(json_obj);
    cJSON_Delete(json_obj);
    ESP_LOGD(WEB_TAG, "%s: Body : %s",__func__, post_data);
    /* Preparar cliente HTTP */
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    /* Realizar petición */
    uint8_t i = 0;
    /* Reintentar y esperar hasta MAX_SEND_ATTEMPTS intentos */
    while( (err = esp_http_client_perform(client)) != ESP_OK && i < MAX_SEND_ATTEMPTS) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (err == ESP_OK) {
        ret.http_status = esp_http_client_get_status_code(client);
        uint8_t res_len = esp_http_client_get_content_length(client);
        ESP_LOGI(WEB_TAG, "%s: HTTP GET Status = %d, content_length = %d",
                __func__, esp_http_client_get_status_code(client), res_len);
        response_body = (char *) reallocarray(response_body, response_len + 1, sizeof(uint8_t));
        esp_log_buffer_hex(WEB_TAG, response_body, response_len);
        if (response_body != NULL) {
            response_body[response_len] = '\0';
            ret.esp_status = ESP_OK; 
            ret.token = response_body;
            ret.token_len = response_len;
        } else {
            ESP_LOGE(WEB_TAG, "%s: Error reordenando memoria para token", __func__);
            free(response_body);
        }
        response_len = 0;
        response_body = NULL;
    } else {
        ESP_LOGE(WEB_TAG, "%s: HTTP GET request failed: %s", __func__, esp_err_to_name(err));
        ret.esp_status = ESP_FAIL;
    }
    esp_http_client_cleanup(client);
    free(post_data);
    return ret;
}

void http_get_token(void) {
    esp_err_t err;
    config.host = "20.121.195.167";
    config.path = "/dev/test";
    //config.path = "/api/nodo_central/test";
    config.port = 8080;
    /* Preparar cliente HTTP */
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    /* Realizar petición */
    uint8_t i = 0;
    /* Reintentar y esperar hasta MAX_SEND_ATTEMPTS intentos */
    while( (err = esp_http_client_perform(client)) != ESP_OK && i < MAX_SEND_ATTEMPTS) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (err == ESP_OK) {
        int res_len = esp_http_client_get_content_length(client);
        ESP_LOGI(WEB_TAG, "%s: HTTP GET Status = %d, content_length = %d",
                __func__, esp_http_client_get_status_code(client), res_len);
        //response_body = (uint8_t *) reallocarray(response_body, response_len + 1, sizeof(uint8_t));
        //if (response_body != NULL) {
        //    response_body[response_len] = '\0';
        //} else {
        //    ESP_LOGE(WEB_TAG, "%s: Error reordenando memoria para token", __func__);
        //    free(response_body);
        //}
    } else {
        ESP_LOGE(WEB_TAG, "%s: HTTP GET request failed: %s", __func__, esp_err_to_name(err));
    }
    ESP_LOG_BUFFER_CHAR(WEB_TAG, response_body, response_len);
    if ( response_body != NULL ) {
        response_len = 0;
        response_body = NULL;
        free(response_body);
    }
    esp_http_client_cleanup(client);
}
esp_err_t nodo_http_init_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(WEB_TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(WEB_TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(WEB_TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(WEB_TAG, "HTTP_EVENT_ON_HEADER");
            ESP_LOGI(WEB_TAG, "%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(WEB_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (response_body == NULL) {
                    response_body = (char *) malloc(esp_http_client_get_content_length(evt->client));
                    response_len = 0;
                    if (response_body == NULL) {
                        ESP_LOGE(WEB_TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                memcpy(response_body + response_len, evt->data, evt->data_len);
                response_len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(WEB_TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(WEB_TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

/* GATTC Init */
static uint16_t const init_char[] = { TEST_DISCOVERY_UUID };

void websocket_task(void *pvParameters) {
    static char headers[512];
    ws_queue_msg_t msg = {0};
    ws_task_arg_t *arg;
    arg = (ws_task_arg_t *) pvParameters;
    // Esperar a que haya conexión WiFI
    ESP_LOGI(WSTASK_TAG, "Esperando conexión WiFi...");
    xEventGroupWaitBits(arg->nodo_evt_group, COMM_CHANNEL_OK, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(WSTASK_TAG, "Preparando WebSocket...");
    /* Acomodar header para token */
    sprintf(headers, "Authorization: %s\r\n", arg->token);
    /*             Importante!   ~~~~~^^^^*/
    // Configurar cliente WebSocket
    esp_websocket_client_config_t websocket_cfg = {
        .user_agent = NODO_USER_AGENT,
        .uri = WEBSOCKET_URI,
        .transport = WEBSOCKET_TRANSPORT_OVER_TCP,
        .ping_interval_sec = WS_PING_INTERVAL,
        .headers = headers,
    };
    esp_websocket_client_handle_t ws_client = esp_websocket_client_init(&websocket_cfg);
    // Registrar handler para WEBSOCKET_EVENT_CONNECTED/DISCONNECTED
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_CONNECTED, ws_evt_handler_conn, (void *) &(arg->nodo_evt_group));
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_DISCONNECTED, ws_evt_handler_conn, (void *) &(arg->nodo_evt_group));
    // Registrar handler para WEBSOCKET_EVENT_DATA/ERROR
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_DATA, ws_evt_handler_data, &(arg->out_queue));
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ERROR, ws_evt_handler_data, NULL); // NULL : user ctx
    esp_websocket_client_start(ws_client);
    while (esp_websocket_client_is_connected(ws_client) != true) {
        ESP_LOGW(WSTASK_TAG, "Esperando conexion con WebSocket..."); 
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    char buffer[JSON_BUFFER_SIZE];
    for(;;) {
        ESP_LOGI(WSTASK_TAG, "Esperando mensajes..");
        xQueueReceive(arg->out_queue, (void *) &msg, portMAX_DELAY);
        memset(buffer, 0, JSON_BUFFER_SIZE);
        if (msg.type == MSG_MEAS_VECTOR) {
            ESP_LOGI(WSTASK_TAG, "MSG_MEAS_VECTOR");
            // TODO: Manejar error donde buffer no puede almacenar JSON
            cJSON *mvector_json = get_measure_vector_json(msg.meas_vector); //(char *) &buffer, JSON_BUFFER_SIZE);
            json_wrap_message_buff(STATUS_OK, SAVE_DATA, mvector_json, buffer, JSON_BUFFER_SIZE );
            ESP_LOGI(WSTASK_TAG, "Enviando mensaje al servidor...");
            ESP_LOGI(WSTASK_TAG, "JSON:\n%s", buffer);
            //ESP_LOGI(WSTASK_TAG, "JSON:\n%s", json_str);
            //ESP_LOG_BUFFER_HEX(WSTASK_TAG, json_str, strlen(json_str));
            esp_websocket_client_send_text(ws_client, buffer, strlen(buffer), portMAX_DELAY);
        }
        if (msg.type == MSG_STATUS_GENERIC) {
            ESP_LOGI(WSTASK_TAG, "MSG_STATUS_GENERIC");
            cJSON *msg_generic_json = get_generic_msg_json(msg.status.esp_status, msg.status.status);
            if ( msg_generic_json != NULL && 
                    cJSON_PrintPreallocated(msg_generic_json, buffer, JSON_BUFFER_SIZE, false) == 0 ) {
                ESP_LOGE(WEB_TAG, "json_wrap_message_buff: Error guardando JSON en buffer!");
            }
            cJSON_Delete(msg_generic_json);
            esp_websocket_client_send_text(ws_client, buffer, strlen(buffer), portMAX_DELAY);
        }
    }
    vTaskDelete(NULL);
}

// Websocket handler para conexión/desconexión
static void ws_evt_handler_conn(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    EventGroupHandle_t *evt_group = (EventGroupHandle_t *) handler_args;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(WSTASK_TAG, "WEBSOCKET_EVENT_CONNECTED");
            xEventGroupSetBits(*evt_group, COMM_DISPATCHER_OK);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(WSTASK_TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            xEventGroupClearBits(*evt_group, COMM_DISPATCHER_OK);
            // TODO: Reactivar conexión
            break;
    }
}

// Websocket handler para intercambio de paquetes de datos
static void ws_evt_handler_data(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    QueueHandle_t *ws_queue = (QueueHandle_t *) handler_args;
    switch (event_id) {
        case WEBSOCKET_EVENT_DATA:
            ESP_LOGI(WSTASK_TAG, "WEBSOCKET_EVENT_DATA");
            ESP_LOGI(WSTASK_TAG, "Received opcode=%d", data->op_code);
            if (data->op_code == 0x08 && data->data_len == 2) {
                ESP_LOGI(WSTASK_TAG, "%s: Received closed message with code=%d"
                        , __func__ ,256*data->data_ptr[0] + data->data_ptr[1]);
            } else if (data->op_code == 0xa) {
                ESP_LOGI(WSTASK_TAG, "%s: Pong", __func__);
            } else {
                ESP_LOGI(WSTASK_TAG, "Received=%.*s", data->data_len, (char *)data->data_ptr);
                cJSON *msg_json = cJSON_ParseWithLength(data->data_ptr, data->data_len);
                if (msg_json == NULL) {
                    ESP_LOGE(WSTASK_TAG, "%s Error parsing message", __func__);
                    break;
                }
                ws_message_handler(msg_json, ws_queue);
                cJSON_Delete(msg_json); 
            }
            //ESP_LOGI(WSTASK_TAG, "Total payload length=%d, data_len=%d, current payload offset=%d", data->payload_len, data->data_len, data->payload_offset);
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGI(WSTASK_TAG, "WEBSOCKET_EVENT_ERROR");
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
void ws_message_handler(cJSON *msg, QueueHandle_t *ws_queue) {
    ESP_LOGI(WSTASK_TAG, "%s", __func__);
    cJSON *msg_type = cJSON_GetObjectItem(msg, "type");
    if ( !cJSON_IsString(msg_type) && msg_type->valuestring == NULL ) {
        ESP_LOGE(WSTASK_TAG, "%s: Error parsing type", __func__);
        return; 
    }
    cJSON *content = cJSON_GetObjectItem(msg, "content");
    if (!cJSON_IsObject(content)) {
        ESP_LOGE(WSTASK_TAG, "%s: Error parsing content", __func__);
        return; 
    }
    if ( strcmp(msg_type->valuestring, "esp-dev-discovery") == 0 ) {
        uint8_t *addr;
        /* Preparar el argumento para el callback */
        cJSON *child = cJSON_GetObjectItem(content, "child");

        if ( ( addr = parse_u8_array(child, ENTRY_LEN) ) == NULL ) {
            return;
        }
        ESP_LOG_BUFFER_HEX(WSTASK_TAG, addr, ENTRY_LEN);
        /* Habilitar el stack BT/BLE sí no esta habilitado */
        if (dev_type == NODO_WIFI) {
            //ESP_LOGI(WSTASK_TAG, "%s| Debería habilitar BT/BLE & iniciar GATTC...", __func__);
            ESP_LOGI(WSTASK_TAG, "%s: Habilitando stack BT/BLE...", __func__);
            if ( nodo_bt_init(ESP_BT_MODE_BLE) != 0 ) {
                ESP_LOGE(WSTASK_TAG, "%s: Error iniciando stack Bluetooth!", __func__);
                return;
            }
            if (init_gatt_client(*ws_queue) != 0) {
                ESP_LOGE(WSTASK_TAG, "%s: Error levantando cliente GATT!", __func__); 
                return;
            }
            register_discovery_cb(gattc_ws_callback);
            // TODO: Agregar str_addr para DISCOVERY_CMPL/FAIL
            char *addr_str = (char *) calloc(MAC_STR_LEN + 1, sizeof(char));
            get_mac_str(addr, addr_str);
            gattc_set_addr(addr, addr_str);
            if ( gattc_set_chars(init_char, sizeof(init_char)/sizeof(init_char[0])) != 0) {
                ESP_LOGE(WSTASK_TAG, "%s: Error declarando chars!", __func__); 
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
        ESP_LOGI(WSTASK_TAG, "%s| Done!", __func__);
        // Discovery...
    } else {
        ESP_LOGW(WSTASK_TAG, "Tipo de msg. no reconocido");
    }
}

/* Callback enviado al proceso de inicialización de BLE */
void gattc_ws_callback(nodo_gattc_events_t evt, void *arg) {
    /* TODO: Emitir mensaje donde falla el proceso de descubrimiento
     * |-> Parcialmente cubierto en nodo_gattc.h! */
    if ( evt == DISCOVERY_CMPL ) {
        /* Guardar en SPIFFS */
        if ( ( dev_type == NODO_WIFI ) &&  ( spiffs_init() != 0 ) ) {
            ESP_LOGE(WSTASK_TAG, "%s| Error inicializando almacenamiento SPIFFS!", __func__);
            return;
        }
        if ( ( spiffs_mounted() != 0 ) && ( spiffs_mount() != 0 ) ) {
            ESP_LOGE(WSTASK_TAG, "%s| Error montando SPIFFS!", __func__);
            return;
        }
        /* TODO: enviar un puntero desde _gattc a los bytes de la dirección MAC */
        if (spiffs_add_entry( (uint8_t *) arg ) != 0 ) {
            ESP_LOGE(WSTASK_TAG, "%s| Error guardando entrada!", __func__);
            return;
        }
        if ( ( dev_type == NODO_WIFI ) && ( nvs_set_mode(SINKNODE) != 0 ) ) {
            ESP_LOGE(WSTASK_TAG, "%s| Error guardando nuevo modo (SINKNODE)!", __func__);
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
        //    ESP_LOGE(WSTASK_TAG, "%s| Error modificando gattc_db!", __func__);
        //else
        //    gattc_db.data = ret;
        ///* Rellenar datos de la última estación agregada */
        //uint8_t *tmp = (uint8_t *) calloc(MAC_BYTES, sizeof(uint8_t));
        //if ( tmp != NULL ) {
        //    memcpy(tmp, arg, MAC_BYTES);
        //} else {
        //    ESP_LOGE(WSTASK_TAG, "%s| Error alojando memoria para raw_addr!", __func__);
        //    return;
        //}
        //gattc_db.data[gattc_db.len - 1].raw_addr = tmp;
        //char *tmp_str = (char *) calloc(MAC_STR_LEN + 1, sizeof(char));
        //get_mac_str(gattc_db.data[gattc_db.len - 1].raw_addr, tmp_str);
        //gattc_db.data[gattc_db.len - 1].str_addr = tmp_str;
        ///* Establecer los parámetros de lectura para GATTC */
        //if ( gattc_set_chars(chars, sizeof(chars)/sizeof(chars[0])) != 0) {
        //    ESP_LOGE(WSTASK_TAG, "%s: Error declarando chars!", __func__); 
        //    return; 
        //}
        unregister_discovery_cb();
    }
}


/*
 * Encapsular el contenido 'content' de un mensaje en un objeto JSON
 */
int json_wrap_message_buff(enum json_msg_status_t status, enum json_msg_type_t type,  cJSON *content, char *buffer, size_t buffer_len) {
    cJSON *json_msg = cJSON_CreateObject();
    cJSON *status_val = cJSON_CreateString(json_get_status_str(status));
    cJSON_AddItemToObject(json_msg, "status", status_val);
    cJSON *type_val = cJSON_CreateString(json_get_msg_type_str(type));
    cJSON_AddItemToObject(json_msg, "type", type_val);
    cJSON_AddItemToObject(json_msg, "content", content);
    if ( cJSON_PrintPreallocated(json_msg, buffer, buffer_len, false) == 0 ) {
        ESP_LOGE(WEB_TAG, "json_wrap_message_buff: Error guardando JSON en buffer!");
        cJSON_Delete(json_msg);
        return -1;
    }
    cJSON_Delete(json_msg);
    return 0;
}

cJSON *get_measure_vector_json(measure_vector_t *mvector) {
    char *timestamp;
    // Crear cadena JSON
    cJSON *json_obj = cJSON_CreateObject();
    timestamp = get_timestamp();
    cJSON *timestamp_val = cJSON_CreateString(timestamp);
    cJSON_AddItemToObject(json_obj, "timestamp", timestamp_val);
    cJSON *dev_addr_val = cJSON_CreateString(mvector->dev_addr);
    cJSON_AddItemToObject(json_obj, "devAddr", dev_addr_val);
    cJSON *meas_array = cJSON_CreateArray();
    cJSON_AddItemToObject(json_obj, "measurements", meas_array);
    for(uint8_t i = 0; i < mvector->len; i++) {
        cJSON *meas_obj = cJSON_CreateObject();
        cJSON_AddItemToArray(meas_array, meas_obj);
        cJSON *type_str = cJSON_CreateString(get_measure_str(mvector->data[i].type));
        cJSON_AddItemToObject(meas_obj, "type", type_str);
        cJSON *val = cJSON_CreateNumber(mvector->data[i].value); 
        cJSON_AddItemToObject(meas_obj, "value", val);
    }
    free(timestamp);
    return json_obj;
}

cJSON *get_generic_msg_json(esp_err_t esp_status, uint8_t status) {
    cJSON *json_msg = cJSON_CreateObject();
    cJSON *status_val = cJSON_CreateString( (esp_status == ESP_OK) ? "ESP_OK" : "ESP_ERR" );
    if (status == DISCOVERY_CMPL) {
        cJSON_AddItemToObject(json_msg, "status", status_val);
        cJSON *type_val = cJSON_CreateString(nodo_gattc_event_to_name(DISCOVERY_CMPL));
        cJSON_AddItemToObject(json_msg, "type", type_val);
    }
    return json_msg;
}
char *get_timestamp() {
    char buffer[128];
    char *timestamp_ptr;
    int timestamp_len;
    time_t now = time(NULL);
    struct tm *now_bd = localtime(&now);
    if ( now_bd == NULL) {
        return NULL;
    }
    if ( strftime(buffer, 64, "%F %T", now_bd) == 0 ) {
        return NULL;
    }
    timestamp_len = strlen(buffer);
    timestamp_ptr = (char *) calloc(timestamp_len + 1, sizeof(char));
    memcpy(timestamp_ptr, buffer, timestamp_len);
    *(timestamp_ptr + timestamp_len) = '\0';
    return timestamp_ptr;
}
