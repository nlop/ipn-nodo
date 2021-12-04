#include "web/http.h"
static char *response_body = NULL;
static int response_len = 0;

static esp_err_t nodo_http_init_handler(esp_http_client_event_t *evt);

/*
 * Configuración del cliente HTTP para enviar el token
 */
static esp_http_client_config_t config = {
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
    ESP_LOGD(HTTP_TAG, "%s: Enviando Token : %s, MAC: %s", __func__, token, mac);
    /* Crear cadena JSON */
    // TODO: Cambiar al nuevo formato gral
    cJSON *json_obj = cJSON_CreateObject();
    if ( json_obj == NULL ) {
        ESP_LOGE(HTTP_TAG, "%s| Error creando objeto JSON", __func__);
        ret.esp_status = ESP_FAIL;
        return ret;
    }
    cJSON *token_val = cJSON_CreateString((char *) token);
    if ( token_val == NULL ) {
        ESP_LOGE(HTTP_TAG, "%s| Error cadena token_val JSON", __func__);
        ret.esp_status = ESP_FAIL;
        return ret;
    }
    cJSON_AddItemToObject(json_obj, "token", token_val);
    char *post_data = cJSON_PrintUnformatted(json_obj);
    cJSON_Delete(json_obj);
    ESP_LOGD(HTTP_TAG, "%s: Body : %s",__func__, post_data);
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
        ESP_LOGI(HTTP_TAG, "%s: HTTP GET Status = %d, content_length = %d",
                __func__, esp_http_client_get_status_code(client), res_len);
        response_body = (char *) reallocarray(response_body, response_len + 1, sizeof(uint8_t));
        //esp_log_buffer_hex(HTTP_TAG, response_body, response_len);
        if (response_body != NULL) {
            response_body[response_len] = '\0';
            ret.esp_status = ESP_OK; 
            ret.token = response_body;
            ret.token_len = response_len;
        } else {
            ESP_LOGE(HTTP_TAG, "%s: Error reordenando memoria para token", __func__);
            free(response_body);
        }
        response_len = 0;
        response_body = NULL;
    } else {
        ESP_LOGE(HTTP_TAG, "%s: HTTP GET request failed: %s", __func__, esp_err_to_name(err));
        ret.esp_status = ESP_FAIL;
    }
    esp_http_client_cleanup(client);
    free(post_data);
    return ret;
}

static esp_err_t nodo_http_init_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_ON_HEADER");
            ESP_LOGI(HTTP_TAG, "%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (response_body == NULL) {
                    response_body = (char *) malloc(esp_http_client_get_content_length(evt->client));
                    response_len = 0;
                    if (response_body == NULL) {
                        ESP_LOGE(HTTP_TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                memcpy(response_body + response_len, evt->data, evt->data_len);
                response_len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}
