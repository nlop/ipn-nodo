#ifndef NODO_QUEUE_H
#define NODO_QUEUE_H

#include <stdint.h>
#include "nodo_wifi.h"
#include "measure/measure_def.h"

/** Message Queues **/
#define SPP_QUEUE_LEN       CONFIG_SPP_QUEUE_LEN
#define WS_QUEUE_LEN        CONFIG_WS_QUEUE_LEN
#define CTRL_QUEUE_LEN      CONFIG_CTRL_QUEUE_LEN
#define GATT_QUEUE_LEN      5

/* TODO: Refactor!!!! */

/** Enums **/
enum spp_msg_type_t { 
    MSG_INIT = 0xF, 
    MSG_SSID,               // 0x10
    MSG_PSK,                // 0x11 
    MSG_SCAN_OK,            // 0x12
    MSG_SCAN_SEND,          // 0x13 
    MSG_SCAN_DONE,          // 0x14 
    MSG_WIFI_CONN_OK,       // 0x15
    MSG_WIFI_CONN_FAIL,     // 0x16
    MSG_TOKEN,              // 0x17
    MSG_TOKEN_ACK,          // 0x18
    MSG_SERV_CONN_OK,       // 0x19
    MSG_SERV_CONN_FAIL,     // 0x1A
    MSG_INIT_BLE,           // 0x1B
    MSG_GATT_OK,            // 0x1C
    MSG_UNKNOWN             // 0x21
};

enum ws_msg_type_t  {
    MSG_MEAS_VECTOR_INST,
    MSG_MEAS_VECTOR_NORM,
    MSG_STATUS_GENERIC
};

enum ctrl_msg_type_t {
    MSG_DEV_DISCOVERY,
    MSG_MEASURE_INST,
    MSG_MEASURE_PH,
};

/** Estructuras **/

typedef struct spp_msg_ssid_t {
    size_t len;
    uint8_t *ssid;
} spp_msg_ssid_t;

typedef struct spp_msg_psk_t {
    size_t len;
    uint8_t *psk;
} spp_msg_psk_t;

typedef struct spp_msg_token_t {
    size_t len;
    uint8_t *token;
} spp_msg_token_t;

typedef struct spp_msg_init_ble_t {
    size_t len;
    uint8_t *instance_id;
} spp_msg_init_ble_t;

typedef struct spp_msg_scan_ok {
    uint8_t ap_scanned;
} spp_msg_scan_ok;

typedef struct spp_msg_t {
    enum spp_msg_type_t type;
    union {
        spp_msg_ssid_t msg_ssid;             // MSG_SSID
        spp_msg_psk_t msg_psk;               // MSG_PSK
        spp_msg_scan_ok msg_scan_ok;         // MSG_SCAN_OK
        nodo_ap_scan_results_t *ap_results;  // MSG_SCAN_SEND
        spp_msg_token_t msg_token;           // MSG_TOKEN
        spp_msg_init_ble_t ble_init;         // MSG_INIT_BLE
    };
} spp_msg_t;

enum esp32_status_msg_t { DEV_DISCOVERY_CMPL };

typedef struct ws_status_generic_t {
    enum esp32_status_msg_t type;
    esp_err_t esp_status;
} ws_status_generic_t;

typedef struct ws_meas_vector_t {
    measure_vector_t *measure;
    const char *dev_addr;
} ws_meas_vector_t;

typedef struct ws_queue_msg_t {
    enum ws_msg_type_t type;
    union {
        ws_meas_vector_t *meas_vector; 
        ws_status_generic_t status;    
    };
} ws_queue_msg_t;

typedef struct ctrl_msg_dev_discovery_t {
    uint8_t *dev_addr;
    uint8_t *instance_id;
} ctrl_msg_dev_discovery_t;

typedef struct ctrl_msg_ {
    const char *dev_addr;
} ctrl_msg_measure_t;

typedef struct ctrl_msg_t {
    enum ctrl_msg_type_t type;
    union {
        /* Dirección para lectura instantanea (MSG_MEASURE_INST) de nodo remoto, 
         * si es NULL, leer sensores integrados */
        ctrl_msg_dev_discovery_t discovery;
        ctrl_msg_measure_t measure;
    };
} ctrl_msg_t;

#endif
