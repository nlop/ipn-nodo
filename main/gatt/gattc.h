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
#include "gatt/gatt_def.h"
#include "nodo_queue.h"
#include "utils/mac.h"

#define GATTC_TAG                   "NODO_GATTC"
#define PROFILE_NUM                 1
#define NODO_PROFILE_ID             0
#define INVALID_HANDLE              0
#define GATTC_SCAN_TIMEOUT          30                  /* Tiempo para escanear servidores BLE */
#define MAX_GATTC_ATTEMPTS          15
#define GATTC_WAIT_START_TIMEOUT    CONFIG_GATTC_WAIT_START_TIMEOUT


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
    uint16_t        handle;
    union {
        uint8_t         *value_str;
        uint16_t        value_u16;
    };
} gattc_char_t;

typedef struct gattc_char_vec_t {
    gattc_char_t        *chars;
    uint8_t             len;
} gattc_char_vec_t;

/** Callbacks **/
typedef void (*gattc_discovery_cb_t) (nodo_gattc_events_t evt, void *arg);

/** Funciones **/
int init_gatt_client(const QueueHandle_t queue);
void gattc_set_addr(const uint8_t *raw_addr, const char *str_addr);
int gattc_set_chars(const uint16_t *chars, uint8_t chars_len);
void gattc_set_instance_id(const uint8_t *inst_id);
int nodo_gattc_start(void);
int gattc_submit_chars(void);
void register_discovery_cb(gattc_discovery_cb_t disco_cb);
void unregister_discovery_cb(void);

#endif
