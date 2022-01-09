#include "init.h"

#define SPP_DISCONNECT_TIMEOUT  5000 // ms para que la aplicacion se desconecte

/** Variables globales **/
uint8_t ssid[MAX_LEN_SSID];
uint8_t psk[MAX_LEN_PSK];


QueueHandle_t queue_in, queue_out;
EventGroupHandle_t nodo_event_group; 

/* Funciones locales */
void close_init_comms(TaskHandle_t send_task, TaskHandle_t recv_task);

// Arrancar la rutina de inicialización del dispositivo
void nodo_init_dev (EventGroupHandle_t event_group) {
    TaskHandle_t recv_task_handle, send_task_handle;
    nodo_event_group = event_group;
    /*** Bluetooth ***/
    if ( nodo_bt_init(ESP_BT_MODE_BTDM) != 0 ) {
        ESP_LOGE(INIT_TAG, "%s: Error iniciando stack Bluetooth!", __func__);
        return;
    }
    spp_init_ret_t ret = nodo_bt_spp_init(nodo_spp_init_recv_cb);
    if ( ret.status != 0 ) {
        ESP_LOGE(INIT_TAG, "nodo_init_dev: Error arrancando BT SPP!");
        return;
    }
    queue_in = ret.in_queue;
    queue_out = ret.out_queue;
    send_task_handle = ret.send_handle;
    // Arrancar conexión WiFi 
    nodo_wifi_init(init_notify_connected_cb, event_group);
    // Arrancar el Task para leer mensajes de inicialización
    init_recv_task_arg_t arg = { .queue_in = queue_in };
    xTaskCreate(
            nodo_init_recv_task, 
            "nodo_recv_init_task", 
            configMINIMAL_STACK_SIZE * 8,
            (void *) &arg,  
            5,
            &recv_task_handle);
    /* Cerar los canales de comunicación de la inicialización (BT SPP)
     * cuando los eventos WIFI_OK o GATT_OK (COMM_CHANNEL_OK) , hayan sido activados */
    xEventGroupWaitBits(event_group, COMM_CHANNEL_OK, pdFALSE, pdTRUE, portMAX_DELAY);
    if (arg.init_dev_type == NODO_WIFI) {
        /* En caso de una conexión web, también esperar el intercambio del token */
        xEventGroupWaitBits(event_group, TOKEN_OK, pdFALSE, pdTRUE, portMAX_DELAY);
    } else {
        spp_msg_t msg = {0};
        msg.type = MSG_GATT_OK;
        xQueueSendToBack(queue_out, &msg, pdMS_TO_TICKS(6000));
    }
    ESP_LOGI(INIT_TAG, "%s: Inicio exitoso : %s", __func__, 
            (arg.init_dev_type == NODO_WIFI ? "NODO_WIFI" : "NODO_BLE" ));
    ESP_LOGI(INIT_TAG, "%s: Listo para cerrar SPP!", __func__);
    if ( nvs_set_mode(arg.init_dev_type) != 0) {
        ESP_LOGD(INIT_TAG, "%s: Error guardando modo de dispositivo!", __func__);
    }
    // Esperar para que la aplicación cierre la conexión de forma ordinaria
    vTaskDelay(pdMS_TO_TICKS(SPP_DISCONNECT_TIMEOUT));
    /* Cerrar canales BT SPP */
    nodo_spp_disable();
    ESP_LOGI(INIT_TAG, "%s: SPP Cerrado!", __func__);
    /* Eliminar los task que atienden el intercambio de datos */
    vTaskDelete(send_task_handle);
    vTaskDelete(recv_task_handle);
    ESP_LOGI(INIT_TAG, "%s: Borrando init tasks!", __func__);
    /* Eliminar las colas de intercambio de mensajes */
    vQueueDelete(queue_in);
    vQueueDelete(queue_out);
    ESP_LOGI(INIT_TAG, "%s: Borrando init queues!", __func__);
    vTaskDelay(pdMS_TO_TICKS(SPP_DISCONNECT_TIMEOUT));
    if (arg.init_dev_type == NODO_WIFI) {
        nodo_bt_disable();
        ESP_LOGI(INIT_TAG, "%s: Bluetooth desactivado!", __func__);
    } else {
        /* Deshabilitar WiFi (iniciado de forma automática al arrancar) */
        if ( nodo_wifi_disable() != 0 ) {
            ESP_LOGE(INIT_TAG, "%s: Error deshabilitando WiFi!", __func__);
        }
    }
    vTaskDelay(pdMS_TO_TICKS(SPP_DISCONNECT_TIMEOUT));
}

void nodo_init_ble_gatts(const EventGroupHandle_t evt_group) {
    if ( nodo_bt_init(ESP_BT_MODE_BLE) != 0 ) {
        ESP_LOGE(INIT_TAG, "%s: Error iniciando stack Bluetooth!", __func__);
        return;
    }
    init_gatt_server(evt_group, NULL);
}

/*
 * Implementación de la capa de aplicación para recibir los mensajes de la cola
 * de entrada (recv) de los mensajes SPP para la inicialización
 */
void nodo_init_recv_task(void *pvParameters) {
    ESP_LOGI(INIT_TAG, "Empezando nodo_recv_init_task...");
    init_recv_task_arg_t *arg = (init_recv_task_arg_t*) pvParameters;
    spp_msg_t msg_buffer = {0};
    BaseType_t queueRet;
    for(;;) {
        queueRet = xQueueReceive(arg->queue_in,  (void *) &msg_buffer, portMAX_DELAY);
        if(queueRet == pdTRUE) {
            ESP_LOGI(INIT_TAG, "Recibiendo de la fila...");
            switch( msg_buffer.type ) {
                case MSG_SSID:
                    memcpy(
                            &ssid, 
                            msg_buffer.msg_ssid.ssid, 
                            (msg_buffer.msg_ssid.len < MAX_LEN_SSID) ? msg_buffer.msg_ssid.len : MAX_LEN_SSID);
                    free(msg_buffer.msg_ssid.ssid);
                    ESP_LOGI(INIT_TAG, "MSG_SSID data: %s, len = %zu", ssid, msg_buffer.msg_ssid.len);
                    break;
                case MSG_PSK:
                    memcpy(
                            &psk, 
                            msg_buffer.msg_psk.psk, 
                            (msg_buffer.msg_psk.len < MAX_LEN_PSK) ? msg_buffer.msg_psk.len : MAX_LEN_PSK);
                    free(msg_buffer.msg_psk.psk);
                    ESP_LOGI(INIT_TAG, "MSG_PSK data: %s, len = %zu", psk, msg_buffer.msg_psk.len);
                    // TODO: Combinar traza de MSG_SSID/PSK en una sola y leer con FSM
                    vTaskDelay(pdMS_TO_TICKS(2500));
                    nodo_wifi_set_credentials(ssid, psk); 
                    break;
                case MSG_INIT:
                    ESP_LOGI(INIT_TAG, "MSG_INIT");
                    arg->init_dev_type = NODO_WIFI;
                    /* Empezar a escanear puntos de acceso (APs) */
                    nodo_ap_scan_results_t ap_results = nodo_wifi_scan();   
                    /* Enviar resultados a la cola de salida (send) */ 
                    send_ap_scanned(&ap_results);
                    break;
                case MSG_INIT_BLE:
                    ESP_LOGI(INIT_TAG, "MSG_INIT_BLE");
                    /* Iniciar el servidor GATT */
                    arg->init_dev_type = NODO_BLE;
                    gatts_instance_id_t instance_id = { 
                        .instance_id = msg_buffer.ble_init.instance_id,
                        .len = msg_buffer.ble_init.len
                    };
                    init_gatt_server(nodo_event_group, &instance_id);
                    break;
                case MSG_TOKEN:
                    ESP_LOGI(INIT_TAG, "MSG_TOKEN");
                    send_init_token(msg_buffer.msg_token.token);
                    // Liberar memoria reservada en nodo_bluetooth.c
                    free(msg_buffer.msg_token.token);
                    break;
                default:
                    ESP_LOGI(INIT_TAG, "Código %d", msg_buffer.type);
            }
        }
    }
    vTaskDelete(NULL);
}

void send_init_token(uint8_t *token) {
    spp_msg_t msg = {0};
    // Confirmar que el token fue recibido
    msg.type = MSG_TOKEN_ACK;
    xQueueSendToBack(queue_out, &msg, portMAX_DELAY);
    memset(&msg, 0, sizeof(spp_msg_t));
    // Recibir el token y mandar una petición web 
    const char *mac_addr = nodo_get_mac();
    /* Esperar para que se obtenga una conexión WiFi */
    xEventGroupWaitBits(nodo_event_group, COMM_CHANNEL_OK, pdFALSE, pdTRUE, portMAX_DELAY);
    token_ret_t ret = http_send_token(token, mac_addr);
    if (ret.esp_status == ESP_OK && ret.http_status == 200) {
        msg.type = MSG_SERV_CONN_OK;
        ESP_LOGI(INIT_TAG, "Conexión con servidor exitosa!");
        //ESP_LOGI(WEB_TAG, "Token: %s [len = %zu]", ret.token, ret.token_len);
        /* Sacar token de JSON obj */
        cJSON *token_json = cJSON_ParseWithLength( (char *) ret.token, ret.token_len);
        if ( token_json == NULL ) {
            ESP_LOGE(INIT_TAG, "%s| Error parsing token_json", __func__);
            return;
        }
        cJSON *token_str = cJSON_GetObjectItem(token_json, "token");
        if ( cJSON_IsString(token_str) && token_str->valuestring != NULL ) {
            if (nvs_save_token(token_str->valuestring) != 0) {
                ESP_LOGE(INIT_TAG, "send_init_token: Error guardando token!");
            }
            ESP_LOGI(INIT_TAG, "Token almacenado exitosamente!");
            // Activar bit TOKEN_OK
            xEventGroupSetBits(nodo_event_group, TOKEN_OK);
            free(ret.token);
        } else {
            ESP_LOGE(INIT_TAG, "%s| Error parsing token_json", __func__);
        }
    } else {
        ESP_LOGI(INIT_TAG, "Conexión con servidor falló!");
        msg.type = MSG_SERV_CONN_FAIL;
    }
    xQueueSendToBack(queue_out, &msg, portMAX_DELAY);
}

/*
 * Enviar los puntos de acceso (APs) almacenados en results,
 * mediante la cola de salida queue_out
 */
void send_ap_scanned(nodo_ap_scan_results_t *results) {
    spp_msg_t msg_aps = {0};
    // Enviar mensaje MSG_SCAN_OK a la fila de salida
    msg_aps.type = MSG_SCAN_OK;
    msg_aps.msg_scan_ok.ap_scanned = results->len;
    xQueueSendToBack(queue_out, &msg_aps, portMAX_DELAY);
    // Limpiar estructura para reuso
    memset(&msg_aps, 0, sizeof(spp_msg_t));
    // Enviar mensaje MSG_SCAN_SEND a la fila de salida
    msg_aps.type = MSG_SCAN_SEND;
    msg_aps.ap_results = results;
    xQueueSendToBack(queue_out, &msg_aps, portMAX_DELAY);
    // Enviar mensaje MSG_SCAN_DONE
    memset(&msg_aps, 0, sizeof(spp_msg_t));
    msg_aps.type = MSG_SCAN_DONE;
    xQueueSendToBack(queue_out, &msg_aps, portMAX_DELAY);
}

/**
 * - Enviar el mensaje de notificación de conexión exitosa a internet,
 * a la aplicación móvil. 
 * - Guardar los datos en la memoria flash
 */
void init_notify_connected_cb(EventGroupHandle_t evt_group, bool connected) {
    ESP_LOGI(INIT_TAG, "Notificando a app");
    spp_msg_t msg_conn_ok = {0};
    if (connected == CONN_OK) {
        ESP_LOGI(INIT_TAG, "Conexión exitosa...");
        msg_conn_ok.type = MSG_WIFI_CONN_OK;
        // Activar bit WIFI_OK (COMM_CHANNEL_OK)
        xEventGroupSetBits(evt_group, COMM_CHANNEL_OK);
        // Guardar SSID/PSK en NVS
        ESP_LOGI(INIT_TAG, "Guardando credenciales...");
        int ret = nvs_save_wifi_credentials((char *) ssid, (char *) psk);
        if ( ret != 0 ) {
            ESP_LOGE(INIT_TAG, "Error guardando credenciales...");
        } else {
            ESP_LOGI(INIT_TAG, "Credenciales guardadas!");
        }
    } else {
        ESP_LOGI(INIT_TAG, "Falló conexión...");
        msg_conn_ok.type = MSG_WIFI_CONN_FAIL;
        xEventGroupClearBits(evt_group, COMM_CHANNEL_OK);
    }
    xQueueSendToBack(queue_out, &msg_conn_ok, portMAX_DELAY);
}

/**
 * Cerrar los canales de comunicación Buetooth SPP utilizados para la
 * inicialización del dispositivo
 * Argumentos:
 *      send_task: Handle obtenido al lanzar el task de envío de datos (capa protocolo)
 *      recv_task: Handle obtenido al lanzar el task de recibo de datos (capa aplicación)
 */
void close_init_comms(TaskHandle_t send_task, TaskHandle_t recv_task) {
    nodo_bt_disable();
}

