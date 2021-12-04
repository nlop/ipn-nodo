#ifndef WEB_JSON_H
#define WEB_JSON_H

#include "esp_log.h"
#include "cJSON.h"
#include "esp_err.h"
#include "utils/time.h"
#include "measure_def.h"
#include "gatt_def.h"

#define JSON_TAG    "JSON"

enum json_msg_status_t {
    STATUS_OK,
    STATUS_ERROR
};

enum json_msg_type_t {
    REQ_LIVE_DATA,
    LIVE_DATA,
    SAVE_DATA
};

cJSON *get_measure_vector_json(measure_vector_t *mvector);
int json_wrap_message_buff(enum json_msg_status_t , enum json_msg_type_t ,  cJSON *, char *, size_t);
cJSON *get_generic_msg_json(esp_err_t esp_status, uint8_t status);
char *json_get_status_str(enum json_msg_status_t status);
char *json_get_msg_type_str(enum json_msg_type_t type);
uint8_t *parse_u8_array(cJSON *array, size_t len);

#endif
