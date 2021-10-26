#include "init.h"

#define MAX_SEND_ATTEMPTS       6    // Numero máximo de intentos para intercambiar el token temporal
#define APP_DISCONNECT_TIMEOUT  5000 // ms para que la aplicacion se desconecte

/** Variables globales **/
uint8_t ssid[MAX_LEN_SSID];
uint8_t psk[MAX_LEN_PSK];


QueueHandle_t queue_in, queue_out;
EventGroupHandle_t nodo_event_group; 

/* Funciones locales */
void close_init_comms(TaskHandle_t send_task, TaskHandle_t recv_task);
void get_mac_str(uint8_t *src, uint8_t *dest);

// Arrancar la rutina de inicialización del dispositivo
void nodo_init_dev (EventGroupHandle_t event_group) {
    TaskHandle_t recv_task_handle, send_task_handle;
    nodo_event_group = event_group;
    /*** Bluetooth ***/
    nodo_bt_init();
    spp_init_ret_t ret = nodo_bt_spp_init(nodo_spp_init_recv_sb);
    if ( ret.status != 0 ) {
        ESP_LOGE(INIT_TAG, "nodo_init_dev: Error arrancando BT SPP!");
        return;
    }
    queue_in = ret.in_queue;
    queue_out = ret.out_queue;
    send_task_handle = ret.send_handle;
    // Arrancar WiFi 
    nodo_wifi_init(init_notify_connected_cb, event_group);
    // Arrancar el Task para leer mensajes de inicialización
    xTaskCreate(
            nodo_init_recv_task, 
            "nodo_recv_init_task", 
            configMINIMAL_STACK_SIZE * 8,
            (void *) &queue_in,  
            5,
            &recv_task_handle);
    // Cerar los canales de comunicación de la inicialización (BT SPP)
    // cuando los eventos WIFI_OK, TOKEN_OK hayan sido activados
    xEventGroupWaitBits(event_group, WIFI_OK | TOKEN_OK,
            pdFALSE,
            pdTRUE,
            portMAX_DELAY);
    ESP_LOGI(INIT_TAG, "nodo_init_dev: Listo para cerrar SPP!");
    // Esperar para que la aplicación cierre la conexión de forma ordinaria
    vTaskDelay(pdMS_TO_TICKS(APP_DISCONNECT_TIMEOUT));
    // Cerrar canales BT SPP de inicialización
    close_init_comms(send_task_handle, recv_task_handle);
}

/*
 * Implementación de la capa de aplicación para recibir los mensajes de la cola
 * de entrada (recv) de los mensajes SPP para la inicialización
 */
void nodo_init_recv_task(void *pvParameters) {
    ESP_LOGI(INIT_TAG, "Empezando nodo_recv_init_task...");
    QueueHandle_t *in_queue = (QueueHandle_t*) pvParameters;
    spp_msg_t msg_buffer = {0};
    BaseType_t queueRet;
    for(;;) {
        queueRet = xQueueReceive(*in_queue,  (void *) &msg_buffer, portMAX_DELAY);
        if(queueRet == pdTRUE) {
            ESP_LOGI(INIT_TAG, "Recibiendo de la fila...");
            switch( msg_buffer.type ) {
                case MSG_SSID:
                    memcpy(
                            &ssid, 
                            msg_buffer.msg_ssid.ssid, 
                            (msg_buffer.msg_ssid.len < MAX_LEN_SSID) ? msg_buffer.msg_ssid.len : MAX_LEN_SSID);
                    free(msg_buffer.msg_ssid.ssid);
                    ESP_LOGI(INIT_TAG, "MSG_SSID data: %s, len = %d", ssid, msg_buffer.msg_ssid.len);
                    break;
                case MSG_PSK:
                    memcpy(
                            &psk, 
                            msg_buffer.msg_psk.psk, 
                            (msg_buffer.msg_psk.len < MAX_LEN_PSK) ? msg_buffer.msg_psk.len : MAX_LEN_PSK);
                    free(msg_buffer.msg_psk.psk);
                    ESP_LOGI(INIT_TAG, "MSG_PSK data: %s, len = %d", psk, msg_buffer.msg_psk.len);
                    // TODO: Combinar traza de MSG_SSID/PSK en una sola y leer con FSM
                    nodo_wifi_set_credentials(ssid, psk); 
                    break;
                case MSG_INIT:
                    ESP_LOGI(INIT_TAG, "MSG_INIT");
                    // Empezar a escanear puntos de acceso (APs) 
                    nodo_ap_scan_results_t ap_results = nodo_wifi_scan();   
                    // Enviar resultados a la cola de salida (send) 
                    send_ap_scanned(&ap_results);
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
    //uint8_t mac_nodo[6];
    //uint8_t mac_nodo_str[32];
    spp_msg_t msg = {0};
    // Confirmar que el token fue recibido
    msg.type = MSG_TOKEN_ACK;
    xQueueSendToBack(queue_out, &msg, portMAX_DELAY);
    memset(&msg, 0, sizeof(spp_msg_t));
    // Recibir el token y mandar una petición web 
    //ESP_ERROR_CHECK(esp_read_mac( (uint8_t*) &mac_nodo, ESP_MAC_BT));
    //get_mac_str((uint8_t *) mac_nodo, (uint8_t *) mac_nodo_str);
    const char *mac_addr = nodo_get_mac();
    uint8_t i = 0;
    while (i < MAX_SEND_ATTEMPTS) {
        token_ret_t ret = http_send_token(token, mac_addr);
        if (ret.esp_status == ESP_OK && ret.http_status == 200) {
            msg.type = MSG_SERV_CONN_OK;
            ESP_LOGI(WEB_TAG, "Conexión con servidor exitosa!");
            ESP_LOGI(WEB_TAG, "Token: %s", ret.token);
            // Activar bit TOKEN_OK
            xEventGroupSetBits(nodo_event_group, TOKEN_OK);
            if (nvs_save_token((char *) ret.token) != 0) {
                ESP_LOGE(INIT_TAG, "send_init_token: Error guardando token!");
            }
            ESP_LOGI(INIT_TAG, "Token almacenado exitosamente!");
            free(ret.token);
            break;
        } else {
            ESP_LOGI(WEB_TAG, "Conexión con servidor falló!");
            msg.type = MSG_SERV_CONN_FAIL;
        }
        ESP_LOGI(WEB_TAG, "Reintentando...");
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

/*
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
        // Activar bit WIFI_OK_EV
        xEventGroupSetBits(evt_group, WIFI_OK);
        // Guardar SSID/PSK en NVS
        ESP_LOGI(INIT_TAG, "Guardando credenciales...");
        // Quitar comentario para guardar credenciales --->
        int ret = nvs_save_wifi_credentials((char *) ssid, (char *) psk);
        if ( ret != 0 ) {
            ESP_LOGE(INIT_TAG, "Error guardando credenciales...");
        } else {
            ESP_LOGI(INIT_TAG, "Credenciales guardadas!");
        }
    } else {
        ESP_LOGI(INIT_TAG, "Falló conexión...");
        msg_conn_ok.type = MSG_WIFI_CONN_FAIL;
        xEventGroupClearBits(evt_group, WIFI_OK);
    }
     xQueueSendToBack(queue_out, &msg_conn_ok, portMAX_DELAY);
}

/*
 * Cerrar los canales de comunicación Buetooth SPP utilizados para la
 * inicialización del dispositivo
 * Argumentos:
 *      send_task: Handle obtenido al lanzar el task de envío de datos (capa protocolo)
 *      recv_task: Handle obtenido al lanzar el task de recibo de datos (capa aplicación)
 */
void close_init_comms(TaskHandle_t send_task, TaskHandle_t recv_task) {
    // Eliminar los task que atienden el intercambio de datos
    vTaskDelete(send_task);
    vTaskDelete(recv_task);
    // Eliminar las colas de intercambio de mensajes
    vQueueDelete(queue_in);
    vQueueDelete(queue_out);
    nodo_spp_disable();
    ESP_LOGI(INIT_TAG, "close_init_comms: BT SPP Cerrado!");
}

