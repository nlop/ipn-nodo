#ifndef NODO_JSON_H
#define  NODO_JSON_H

#include <stdio.h>
#include "esp_log.h"
#include "cJSON.h"

#define JSON_TAG    "NODO_JSON"

enum json_msg_status_t {
    STATUS_OK,
    STATUS_ERROR
};

enum json_msg_type_t {
    REQ_LIVE_DATA,
    LIVE_DATA,
    SAVE_DATA
};

char *json_get_status_str(enum json_msg_status_t status);
char *json_get_msg_type_str(enum json_msg_type_t type);
uint8_t *parse_u8_array(cJSON *array, size_t len);

#endif 
