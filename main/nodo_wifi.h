#ifndef NODO_WIFI_H
#define NODO_WIFI_H

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_err.h"

#define WIFI_TAG                "NODO-WIFI"
#define MAX_AP_SCAN_SIZE        CONFIG_MAX_AP_SCAN_SIZE
#define MAX_SCAN_ATTEMPTS       CONFIG_MAX_SCAN_ATTEMPTS
#define MAX_CONNECT_ATTEMPTS    CONFIG_MAX_CONNECT_ATTEMPTS
#define CONN_OK                 true
#define CONN_FAIL               false

enum NODO_WIFI_AUTH_TYPE { 
    NODO_WIFI_AUTH_OPEN = 0x30, 
    NODO_WIFI_AUTH_WEP,             // 0x31
    NODO_WIFI_AUTH_WPA_PSK,         // 0x32 
    NODO_WIFI_AUTH_WPA2_PSK,        // 0x33
    NODO_WIFI_AUTH_WPA_WPA2_PSK,    // 0x34
    NODO_WIFI_AUTH_WPA3_PSK,        // 0x35
    NODO_WIFI_AUTH_WPA2_WPA3_PSK,   // 0x36
    NODO_AUTH_UNKNOWN               // 0x37
};

/** Tipos **/
typedef void (*nodo_wifi_conn_cb_t) (EventGroupHandle_t, bool);
typedef void (*nodo_wifi_cb_t) (void);

/** Estructuras **/
typedef struct nodo_ap_scan_results_t {
    uint8_t len;
    wifi_ap_record_t *results;
} nodo_ap_scan_results_t;

typedef struct wifi_event_handler_arg_t {
    nodo_wifi_conn_cb_t on_conn_cb;
    EventGroupHandle_t event_group;
} wifi_event_handler_arg_t;

/** Funciones **/
void nodo_wifi_init(nodo_wifi_conn_cb_t on_conn_cb, EventGroupHandle_t evt_group);
nodo_ap_scan_results_t nodo_wifi_scan();
uint8_t get_auth_mode(int authmode);
void nodo_wifi_set_credentials(uint8_t *ssid, uint8_t *psk);
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
int nodo_wifi_disable(void);
#endif
