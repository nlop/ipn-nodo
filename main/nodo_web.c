#include "nodo_web.h"

esp_err_t nodo_http_init_handler(esp_http_client_event_t *evt);
uint8_t *response_body = NULL;
uint8_t response_len = 0;
/** Funciones locales **/
static void ws_evt_handler_conn(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void ws_evt_handler_data(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
cJSON *get_measure_vector_json(measure_vector_t *mvector, const char *dev_addr);
int json_wrap_message_buff(enum json_msg_status_t , enum json_msg_type_t ,  cJSON *, char *, size_t);
char *get_timestamp();


/*
 * Configuración del cliente HTTP para enviar el token
 */
esp_http_client_config_t config = {
    .url = TOKEN_INIT_URL,
    .event_handler = nodo_http_init_handler,
    .user_agent = NODO_USER_AGENT,
#ifdef HTTP_PORT
    .port = HTTP_PORT
#endif
};

/*
 * Enviar el token temporal mandado por Bluetooth al servidor y esperar a recibir el token de
 * conexión de verdad, enviado por HTTPS
 */
token_ret_t http_send_token(uint8_t *token, const char *mac) {
    token_ret_t ret = {0};
    ESP_LOGI(WEB_TAG, "Enviando Token : %s, MAC: %s", token, mac);
    // Crear cadena JSON
    // TODO: Cambiar al nuevo formato gral
    cJSON *json_obj = cJSON_CreateObject();
    cJSON *token_val = cJSON_CreateString((char *) token);
    cJSON_AddItemToObject(json_obj, "token", token_val);
    cJSON *mac_val = cJSON_CreateString(mac);
    cJSON_AddItemToObject(json_obj, "mac", mac_val);
    char *post_data = cJSON_PrintUnformatted(json_obj);
    cJSON_Delete(json_obj);
    ESP_LOGI(WEB_TAG, "Body : %s", post_data);
    // Preparar cliente HTTP
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    // Realizar petición
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ret.http_status = esp_http_client_get_status_code(client);
        uint8_t res_len = esp_http_client_get_content_length(client);
        ESP_LOGI(WEB_TAG, "HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client), res_len);
        //esp_log_buffer_hex(WEB_TAG, response_body, response_len);
        response_body = (uint8_t *) reallocarray(response_body, response_len + 1, sizeof(uint8_t));
        if (response_body != NULL) {
            response_body[response_len] = '\0';
            ret.esp_status = ESP_OK; 
            ret.token = response_body;
        } else {
            ESP_LOGE(WEB_TAG, "Error reordenando memoria para token");
            free(response_body);
        }
        response_len = 0;
        response_body = NULL;
    } else {
        ESP_LOGE(WEB_TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        ret.esp_status = ESP_FAIL;
    }
    free(post_data);
    esp_http_client_cleanup(client);
    return ret;
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
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(WEB_TAG, "HTTP_EVENT_ON_DATA");
            if (response_body == NULL) {
                response_len = 0;
                response_body = (uint8_t *) malloc(esp_http_client_get_content_length(evt->client));
                if (response_body == NULL) {
                    ESP_LOGE(WEB_TAG, "Failed to allocate memory for output buffer");
                    return ESP_FAIL;
                }
            }
            memcpy(response_body + response_len, evt->data, evt->data_len);
            response_len += evt->data_len;
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

void websocket_task(void *pvParameters) {
    ws_queue_msg_t msg = {0};
    ws_task_arg_t *arg;
    arg = (ws_task_arg_t *) pvParameters;
    // Esperar a que haya conexión WiFI
    ESP_LOGI(WSTASK_TAG, "Esperando conexión WiFi...");
    xEventGroupWaitBits(arg->nodo_evt_group, WIFI_OK, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(WSTASK_TAG, "Preparando WebSocket...");
    // Configurar cliente WebSocket
    esp_websocket_client_config_t websocket_cfg = { .user_agent = NODO_USER_AGENT };
    websocket_cfg.uri = WEBSOCKET_URL;
#ifdef WEBSOCKET_PORT
    websocket_cfg.port = WEBSOCKET_PORT;
#endif
    esp_websocket_client_handle_t ws_client = esp_websocket_client_init(&websocket_cfg);
    // Registrar handler para WEBSOCKET_EVENT_CONNECTED/DISCONNECTED
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_CONNECTED, ws_evt_handler_conn, (void *) &(arg->nodo_evt_group));
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_DISCONNECTED, ws_evt_handler_conn, (void *) &(arg->nodo_evt_group));
    // Registrar handler para WEBSOCKET_EVENT_DATA/ERROR
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_DATA, ws_evt_handler_data, NULL);
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ERROR, ws_evt_handler_data, NULL);
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
            cJSON *mvector_json = get_measure_vector_json(msg.meas_vector, nodo_get_mac()); //(char *) &buffer, JSON_BUFFER_SIZE);
            json_wrap_message_buff(STATUS_OK, LIVE_DATA, mvector_json, buffer, JSON_BUFFER_SIZE );
            //int len = sprintf(buffer, "\{\"timestamp\":\"1998-02-11\",\"measurements\":\[\{\"type\":\"temperature\",\"value\":%d},\{\"type\":\"humidity\",\"value\":%d}]}", msg.meas_vector->data[0].value, msg.meas_vector->data[1].value);
            ESP_LOGI(WSTASK_TAG, "Enviando mensaje al servidor...");
            ESP_LOGI(WSTASK_TAG, "JSON:\n%s", buffer);
            //ESP_LOGI(WSTASK_TAG, "JSON:\n%s", json_str);
            //ESP_LOG_BUFFER_HEX(WSTASK_TAG, json_str, strlen(json_str));
            esp_websocket_client_send_text(ws_client, buffer, strlen(buffer), portMAX_DELAY);
            //esp_websocket_client_send_bin(ws_client, (char *) &json_str, strlen(json_str), portMAX_DELAY);
            //free(json_str);
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
            xEventGroupSetBits(*evt_group, HTTP_OK);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(WSTASK_TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            xEventGroupClearBits(*evt_group, HTTP_OK);
            // TODO: Reactivar conexión
            break;
    }
}

// Websocket handler para intercambio de datos
static void ws_evt_handler_data(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(WSTASK_TAG, "WEBSOCKET_EVENT_DATA");
        ESP_LOGI(WSTASK_TAG, "Received opcode=%d", data->op_code);
        if (data->op_code == 0x08 && data->data_len == 2) {
            ESP_LOGW(WSTASK_TAG, "Received closed message with code=%d", 256*data->data_ptr[0] + data->data_ptr[1]);
        } else {
            ESP_LOGW(WSTASK_TAG, "Received=%.*s", data->data_len, (char *)data->data_ptr);
        }
        ESP_LOGW(WSTASK_TAG, "Total payload length=%d, data_len=%d, current payload offset=%d", data->payload_len, data->data_len, data->payload_offset);
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(WSTASK_TAG, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

/*
 * Convertir un vector de mediciones (measure_vector_t) en un objeto JSON
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

cJSON *get_measure_vector_json(measure_vector_t *mvector, const char *dev_addr) {
    char *timestamp;
    // Crear cadena JSON
    cJSON *json_obj = cJSON_CreateObject();
    timestamp = get_timestamp();
    cJSON *timestamp_val = cJSON_CreateString(timestamp);
    cJSON_AddItemToObject(json_obj, "timestamp", timestamp_val);
    cJSON *dev_addr_val = cJSON_CreateString(dev_addr);
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
