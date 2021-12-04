#ifndef WEB_COMMON_H
#define WEB_COMMON_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_websocket_client.h"
#include "esp_event.h"
#include "cJSON.h"
#include "nodo_queue.h"

#define NODO_USER_AGENT     CONFIG_WEB_USER_AGENT
#define SERVER_URL          CONFIG_SERVER_URL

#endif
