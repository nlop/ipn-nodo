#ifndef WEB_WEBSOCKET_H
#define WEB_WEBSOCKET_H

#include "esp_websocket_client.h"
#include "esp_event.h"
#include "web/web_common.h"
#include "web/json.h"
#include "utils/mac.h"
#include "esp32/nodo_events.h"

#define WEBSOCK_TAG         "WEBSOCKET"
#define WS_PING_INTERVAL    CONFIG_WEBSOCKET_PING_INT
#define WEBSOCKET_PORT      CONFIG_WEBSOCKET_PORT
#define JSON_BUFFER_SIZE    384
#define HEADER_BUFFER_LEN   512

typedef struct ws_task_arg_t {
    QueueHandle_t out_queue;                /* Cola para recibir paquetes enviados por otros tasks */
    QueueHandle_t ctrl_queue;               /* Cola para recibir mensajes de control */
    EventGroupHandle_t nodo_evt_group;      /* Handle al event group de todo el nodo. Usado para
                                             * alzar el evento HTTP_OK (COMM_CHANNEL_OK) y esperar WIFI_OK (COMM DISPATCHER_OK) */
    uint8_t *token;                         /* Token para comunicarse de forma segura con los servicios web */
} ws_task_arg_t;

typedef struct ws_msg_handler_arg_t {
    QueueHandle_t out_queue;                /* Cola para recibir paquetes enviados por otros tasks */
    QueueHandle_t ctrl_queue;               /* Cola para recibir mensajes de control */
} ws_msg_handler_arg_t;

void websocket_task(void *pvParameters);

#endif
