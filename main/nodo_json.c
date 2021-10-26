#include "nodo_json.h"

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
