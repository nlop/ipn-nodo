#include "gatt_def.h"

const char *nodo_gattc_event_to_name(nodo_gattc_events_t evt) {
    switch (evt) {
        case DISCOVERY_CMPL: 
            return "discovery-cmpl";
            break;
    }
    return NULL;
}

