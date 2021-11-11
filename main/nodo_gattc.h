#ifndef NODO_GATTC_H
#define NODO_GATTC_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define GATTC_TAG                   "GATTC_DEMO"
#define REMOTE_SERVICE_UUID         0x3931
#define REMOTE_NOTIFY_CHAR_UUID     0xFF01
#define TEMP_CHAR_UUID              0x2A6E
#define PROFILE_NUM                 1
#define NODO_PROFILE_ID             0
#define INVALID_HANDLE              0

/** Enums **/
typedef enum {DISCOVERY_CMPL} nodo_gattc_events_t;


struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle; // <-- Rango de handles 
    uint16_t service_end_handle;   // <--/
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
};

/** Callbacks **/
typedef void (*gattc_discovery_cb_t) (nodo_gattc_events_t evt, void* arg);

/** Funciones **/
void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
int init_gatt_client(gattc_discovery_cb_t disco_cb, void *cb_arg);
const char *nodo_gattc_event_to_name(nodo_gattc_events_t evt);

#endif
