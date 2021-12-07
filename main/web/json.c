#include "json.h"

cJSON *get_measure_vector_json(measure_vector_t *mvector) {
    char *timestamp;
    // Crear cadena JSON
    cJSON *json_obj = cJSON_CreateObject();
    if ( json_obj == NULL ) {
        ESP_LOGE(JSON_TAG, "%s| Error creando objeto JSON!", __func__);
        return NULL;
    }
    timestamp = get_timestamp();
    if ( timestamp == NULL ) {
        ESP_LOGE(JSON_TAG, "%s| Error obteniendo hora!", __func__);
        cJSON_Delete(json_obj);
        return NULL;
    }
    cJSON *timestamp_val = cJSON_CreateString(timestamp);
    if ( timestamp_val == NULL ) {
        ESP_LOGE(JSON_TAG, "%s| Error creando objeto JSON: timestamp!", __func__);
        cJSON_Delete(json_obj);
        free(timestamp);
        return NULL;
    }
    cJSON_AddItemToObject(json_obj, "timestamp", timestamp_val);
    cJSON *meas_array = cJSON_CreateArray();
    if ( meas_array == NULL ) {
        ESP_LOGE(JSON_TAG, "%s| Error creando objeto JSON:measArray!", __func__);
        cJSON_Delete(json_obj);
        cJSON_Delete(timestamp_val);
        free(timestamp);
        return NULL;
    }
    cJSON_AddItemToObject(json_obj, "measurements", meas_array);
    for(uint8_t i = 0; i < mvector->len; i++) {
        cJSON *meas_obj = cJSON_CreateObject();
        if ( meas_obj == NULL ) {
            ESP_LOGE(JSON_TAG, "%s| Error creando objeto JSON:measObj!", __func__);
            cJSON_Delete(meas_array);
            cJSON_Delete(json_obj);
            cJSON_Delete(timestamp_val);
            free(timestamp);
            return NULL;
        }
        cJSON_AddItemToArray(meas_array, meas_obj);
        cJSON *type_str = cJSON_CreateString(get_meas_type_str(mvector->data[i].type));
        cJSON_AddItemToObject(meas_obj, "type", type_str);
        cJSON *val = NULL;
        if ( mvector->data[i].type == HUMIDITY) {
            float hum_perc = ( -10000.0 * mvector->data[i].value * 0.001 + 20800 ) / 133;
            val = cJSON_CreateNumber(hum_perc); 
        } else if ( mvector->data[i].type == TEMPERATURE) {
            float temp_float = mvector->data[i].value * 0.1;
            val = cJSON_CreateNumber(temp_float); 
        } else {
            val = cJSON_CreateNumber(mvector->data[i].value); 
        }
        if ( val != NULL ) {
            cJSON_AddItemToObject(meas_obj, "value", val);
        }
    }
    free(timestamp);
    return json_obj;
}

/*
 * Encapsular el contenido 'content' de un mensaje en un objeto JSON
 */
int json_wrap_message_buff(const char *dev_addr, enum json_msg_status_t status, enum json_msg_type_t type,  cJSON *content, char *buffer, size_t buffer_len) {
    cJSON *json_msg = cJSON_CreateObject();
    if ( dev_addr != NULL ) {
        cJSON *addr_str = cJSON_CreateString(dev_addr);
        if ( addr_str == NULL ) return -1;
        cJSON_AddItemToObject(json_msg, "devAddr", addr_str);
    }
    cJSON *status_val = cJSON_CreateString(json_get_status_str(status));
    cJSON_AddItemToObject(json_msg, "status", status_val);
    cJSON *type_val = cJSON_CreateString(json_get_msg_type_str(type));
    cJSON_AddItemToObject(json_msg, "type", type_val);
    if ( content != NULL ) {
        cJSON_AddItemToObject(json_msg, "content", content);
    }
    if ( cJSON_PrintPreallocated(json_msg, buffer, buffer_len, false) == 0 ) {
        ESP_LOGE(JSON_TAG, "json_wrap_message_buff: Error guardando JSON en buffer!");
        cJSON_Delete(json_msg);
        return -1;
    }
    cJSON_Delete(json_msg);
    return 0;
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

uint8_t *parse_u8_array(cJSON *array, size_t len) {
    ESP_LOGI(JSON_TAG, "%s", __func__);
    uint8_t *addr = NULL;
    uint8_t i = 0;
    cJSON *item = NULL;
    if ( !cJSON_IsArray(array) ) {
        ESP_LOGE(JSON_TAG, "%s| Error parsing child!", __func__);
    } else {
        addr = (uint8_t *) calloc(len, sizeof(uint8_t));
        if ( addr == NULL ) {
            ESP_LOGE(JSON_TAG, "%s| Error no hay memoria!", __func__);
            return NULL;
        }
        cJSON_ArrayForEach(item, array){
            if (!cJSON_IsNumber(item) ) {
                ESP_LOGE(JSON_TAG, "%s: Error parsing child (addr)!", __func__);
                free(addr);
                return NULL;
            }
            if (i < len)
                addr[i++] = (uint8_t ) item->valueint;
        }
    }
    return addr;
}

char *json_get_status_str(enum json_msg_status_t status) {
    switch(status) {
        case STATUS_OK:
            return "ok";
        case STATUS_ERROR:
            return "error";
        default:
            return NULL;
    };
}

char *json_get_msg_type_str(enum json_msg_type_t type) {
    switch(type) {
        case REQ_LIVE_DATA:
            return "req-live-data";
        case LIVE_DATA:
            return "live-data";
        case SAVE_DATA:
            return "save-data";
        default:
            return NULL;
    };
}
