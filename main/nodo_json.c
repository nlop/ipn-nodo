#include "nodo_json.h"

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
            //ESP_LOGI(JSON_TAG, "0x%02x", item->valueint);
            if (i < len)
                addr[i++] = (uint8_t ) item->valueint;
                //ESP_LOGI(JSON_TAG, "i = %d", i++);
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
