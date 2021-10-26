#ifndef INIT_H
#define INIT_H
#include <string.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nodo_queue.h"
#include "nodo_wifi.h"
#include "nodo_bluetooth.h"
#include "nodo_web.h"
#include "nodo_events.h"
#include "nodo_nvs.h"
#include "nodo_mac.h"


#define INIT_TAG        "NODO INIT"
#define MAX_LEN_SSID    32          // Longitud máx. del SSID según el protocolo 802.11
#define MAX_LEN_PSK     64          // Longitud máx. para la PSK según el protocolo 802.11

/** Funciones **/
void nodo_init_dev(EventGroupHandle_t event_group);
void nodo_init_recv_task(void *pvParameters);
void send_ap_scanned(nodo_ap_scan_results_t *results);
void init_notify_connected_cb(EventGroupHandle_t evt_group, bool connected);
void send_init_token(uint8_t *token);

#endif
