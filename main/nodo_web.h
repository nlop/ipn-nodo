#ifndef NODO_WEB_H
#define NODO_WEB_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_websocket_client.h"
#include "esp_event.h"
#include "cJSON.h"
#include "nodo_events.h"
#include "nodo_def.h"
#include "nodo_queue.h"
#include "nodo_json.h"
#include "nodo_mac.h"
#include "nodo_bluetooth.h"
#include "nodo_gattc.h"
#include "nodo_spiffs.h"
#include "nodo_nvs.h"
#include "esp_system.h"

#define WEB_TAG             "NODO WEB"
#define WSTASK_TAG          "WS TASK"
#define NODO_USER_AGENT     CONFIG_WEB_USER_AGENT
#define SERVER_URL          CONFIG_SERVER_URL
#define HTTP_HOST           SERVER_URL
#define INIT_PATH           "/dev/new"
#define WEBSOCKET_HOST      SERVER_URL

#if CONFIG_WEBSOCKET_PORT != 80
#define WEBSOCKET_PORT      CONFIG_WEBSOCKET_PORT
#endif

#if CONFIG_HTTP_PORT != 80
#define HTTP_PORT           CONFIG_HTTP_PORT
#endif

#define JSON_BUFFER_SIZE    256

/** Tipos **/
typedef void (*token_revc_cb_t)(uint8_t *token);

/** Estructuras **/
typedef struct token_ret_t {
    esp_err_t esp_status;
    uint8_t http_status;
    uint8_t *token;
} token_ret_t;



typedef struct ws_task_arg_t {
    QueueHandle_t out_queue;                /* Cola para recibir paquetes enviados por otros tasks */
    EventGroupHandle_t nodo_evt_group;      /* Handle al event group de todo el nodo. Usado para
                                             * alzar el evento HTTP_OK (COMM_CHANNEL_OK) y esperar WIFI_OK (COMM DISPATCHER_OK) */
    uint8_t *token;                         /* Token para comunicarse de forma segura con los servicios web */
} ws_task_arg_t;

/** Funciones **/
token_ret_t http_send_token(uint8_t *token, const char *mac);
void websocket_task(void *pvParameters);
void gattc_ws_callback(nodo_gattc_events_t evt, void *arg);

    
#endif
