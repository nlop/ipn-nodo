#ifndef NODO_GATTC_H
#define NODO_GATTC_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nodo_queue.h"

#define GATTC_TAG                   "NODO_GATTC"
#define PROFILE_NUM                 1
#define NODO_PROFILE_ID             0
#define INVALID_HANDLE              0
#define MAC_ADDR_LEN                6
#define GATTC_SCAN_TIMEOUT          30                  /* Tiempo para escanear servidores BLE */
#define GATTC_WAIT_START_TIMEOUT    3000
/* TODO: Pasar a config ^^^^ */

/* TODO: Concentrar UUIDs en esp_def.h */

/** UUIDs importantes **/
#define REMOTE_SERVICE_UUID         0x3931
#define REMOTE_NOTIFY_CHAR_UUID     0xFF01
#define TEMP_CHAR_UUID              0x2A6E
#define TEST_DISCOVERY_UUID         0x2a23
#define HUMIDITY_CHAR_UUID          0x2A6F
#define LUX_CHAR_UUID               0x2AFB


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

typedef struct gattc_ws_init_arg_t {
    QueueHandle_t ws_queue;
    uint8_t *server_addr;
} gattc_ws_init_arg_t;

typedef struct gattc_char_t {
    esp_bt_uuid_t   uuid;
    uint16_t        value;
    uint16_t        handle;
} gattc_char_t;

typedef struct gattc_char_vec_t {
    gattc_char_t        *chars;
    uint8_t             len;
} gattc_char_vec_t;

/** Callbacks **/
typedef void (*gattc_discovery_cb_t) (nodo_gattc_events_t evt, void *arg);

/** Funciones **/
void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
int init_gatt_client(const QueueHandle_t queue);
const char *nodo_gattc_event_to_name(nodo_gattc_events_t evt);
uint16_t u16_from_bytes(const uint8_t *bytes, uint8_t len);
void gattc_set_addr(const uint8_t *raw_addr, const char *str_addr);
int gattc_set_chars(const uint16_t *chars, uint8_t chars_len);
int nodo_gattc_start(void);
int gattc_submit_chars(void);
void register_discovery_cb(gattc_discovery_cb_t disco_cb);
void unregister_discovery_cb(void);

#endif
