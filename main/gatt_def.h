#ifndef GATT_DEF_H
#define GATT_DEF_H
#include <stdio.h>

/** Enums **/
typedef enum {DISCOVERY_CMPL} nodo_gattc_events_t;

const char *nodo_gattc_event_to_name(nodo_gattc_events_t evt);

#endif
