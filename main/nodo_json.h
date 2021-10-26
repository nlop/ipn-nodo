#ifndef NODO_JSON_H
#define  NODO_JSON_H

#include <stdio.h>

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

#endif 
