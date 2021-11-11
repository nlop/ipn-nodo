#ifndef NODO_BLUETOOTH_H
#define NODO_BLUETOOTH_H

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nodo_queue.h"
#include "nodo_gatts.h"

#define BT_TAG          "NODO BLUETOOTH"
#define BT_DEVICE_NAME  "IPN-NODO"


/** Tipos **/
typedef void (*nodo_spp_recv_cb) (QueueHandle_t queue, esp_spp_cb_param_t *param);

/** Estructuras **/
typedef struct byte_buffer_t {
    size_t len;
    uint8_t *data;
} byte_buffer_t;

typedef struct spp_init_ret_t {
    int status;
    QueueHandle_t in_queue;
    QueueHandle_t out_queue;
    TaskHandle_t send_handle;
} spp_init_ret_t;

/** Funciones **/
int nodo_bt_init(esp_bt_mode_t mode);
spp_init_ret_t nodo_bt_spp_init(nodo_spp_recv_cb recv_cb);
void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
void nodo_init_read_task(void *pvParameters);
void nodo_spp_init_recv_cb(QueueHandle_t queue, esp_spp_cb_param_t *param);
uint8_t *copy_msg(uint8_t len, uint8_t *src);
void nodo_init_send_task(void *pvParameters);
byte_buffer_t nodo_ap_data_prepare(wifi_ap_record_t *ap_record);
void nodo_bt_disable(void);
void nodo_spp_disable(void);

#endif
