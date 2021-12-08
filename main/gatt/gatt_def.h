#ifndef GATT_DEF_H
#define GATT_DEF_H
#include <stdio.h>

/** UUIDs del servicio **/

#define REMOTE_SERVICE_UUID         0x3931
#define REMOTE_NOTIFY_CHAR_UUID     0xFF01
#define TEMP_CHAR_UUID              0x2A6E
#define TEST_DISCOVERY_UUID         0x2a23
#define HUMIDITY_CHAR_UUID          0x2A6F
#define LUX_CHAR_UUID               0x2AFB
#define INSTANCE_ID_VAL_LEN         16

/** Enums **/
typedef enum {DISCOVERY_CMPL} nodo_gattc_events_t;


const char *nodo_gattc_event_to_name(nodo_gattc_events_t evt);

#endif
