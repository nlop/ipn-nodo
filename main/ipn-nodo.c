#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "init.h"
#include "nodo_nvs.h"
#include "nodo_web.h"
#include "measure.h"
#include "esp_sntp.h"
#include "nodo_mac.h"
#include "nodo_ble.h"

#define MAIN_TAG "MAIN"
#define GATT_MODE   true
enum conn_type_t { CONN_TYPE_GATT, CONN_TYPE_WEBSOCKET };

// Declaraciones
int start(enum conn_type_t dev_type, const EventGroupHandle_t evt_group);
void start_notify_connected_cb(EventGroupHandle_t evt_group, bool connected);
void sync_time();

void app_main(void) {
    /* TODO:
     *     + Sí se conecta a WiFi pero aún no consigue el Token, reinciar todo el proceso
     *          => Solución parcial al guardar el Token
     *     + Configuración de controlador WiFi de acuerdo al país
     *     + Manejar caso donde la estación ya esta inicializada y no
     *      se puede conectar con las credenciales guardadas [X]
     */
    // TODO: Revisar función
    //nodo_get_mac();
    // Inicializar y verificar almacenamiento no-volatil (NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(MAIN_TAG, "Inicializando NVS");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK( ret );
    // Crear EventGroup para eventos del nodo
    EventGroupHandle_t nodo_evt_group = xEventGroupCreate();
    if (nodo_evt_group == NULL) {
        ESP_LOGE(MAIN_TAG, "app_main: Error creando event group");
        return;
    }
#if GATT_MODE
    ESP_LOGI(MAIN_TAG, "Inicializando BLE GATT mode!");
    nodo_init_ble(nodo_evt_group); 
    ESP_LOGI(MAIN_TAG, "app_main: Arrancando tareas...");
    start(CONN_TYPE_GATT, nodo_evt_group);
#else
    ESP_LOGI(MAIN_TAG, "Inicializando WiFi mode!");
    /*
     * Buscar en la NVS las llaves NVS_AP_PART.AP_SSID/AP_PSK para saber si la
     * estación ha sido inicializada anteriormente
     */
    uint8_t *ssid, *psk;
    if ( nvs_get_wifi_credentials(&ssid, &psk) == 0 ) {
        ESP_LOGI(MAIN_TAG, "Credenciales guardadas %s : %s", ssid, psk);
        ESP_LOGI(MAIN_TAG, "Inicializando estación: normal boot");
        nodo_wifi_init(start_notify_connected_cb, nodo_evt_group);
        nodo_wifi_set_credentials(ssid, psk);
    } else {
        ESP_LOGI(MAIN_TAG, "No hay credenciales guardadas!\nInicializando estación: first boot...");
        nodo_init_dev(nodo_evt_group);
    }
    // Sincronizar el tiempo con NTP
    sync_time();
    ESP_LOGI(MAIN_TAG, "app_main: Arrancando tareas...");
    start(CONN_TYPE_WEBSOCKET, nodo_evt_group);
#endif
    ESP_LOGI(MAIN_TAG, "app_main: Listo!");
}

/*
 * Arrancar las tareas normales de envío de datos y medición
 *
 * Argumentos:
 *      evt_group: Handle para el grupo de eventos que marca eventos relevantes
 *      del nodo
 */
int start(enum conn_type_t conn_type, const EventGroupHandle_t evt_group) {
    QueueHandle_t queue_out;
    TaskHandle_t ws_task_handle, meas_task_handle;
    BaseType_t ret;
    if (conn_type == CONN_TYPE_WEBSOCKET) {
        ESP_LOGD(MAIN_TAG, "Arrancado task WebSocket...");
        // Leer token para servicios web
        uint8_t *token;
        if ( (token = nvs_get_token()) == NULL) {
            ESP_LOGE(MAIN_TAG, "start: Error leyendo token");
            return -1;
        }
        queue_out = xQueueCreate(WS_QUEUE_LEN, sizeof(ws_queue_msg_t));
        // Arrancar Task WEBSOCKET 
        ws_task_arg_t *ws_arg = (ws_task_arg_t *) calloc(1, sizeof(ws_task_arg_t));
        ws_arg->token = token;
        ws_arg->nodo_evt_group = evt_group;
        ws_arg->out_queue = queue_out;
        ret = xTaskCreate(
                websocket_task, 
                "websocket_task", 
                configMINIMAL_STACK_SIZE * 4,
                (void *) ws_arg,
                5,
                &ws_task_handle);
        if (ret == pdFALSE) {
            ESP_LOGE(MAIN_TAG, "%s: Error creando websocket_task", __func__);
            return -1;
        }
    } else {
        ESP_LOGD(MAIN_TAG, "Arrancado task GATT...");
        queue_out = xQueueCreate(GATT_QUEUE_LEN, sizeof(ws_queue_msg_t));
        gatt_task_arg_t *gatt_arg = (gatt_task_arg_t *) calloc(1, sizeof(gatt_task_arg_t));
        gatt_arg->nodo_evt_group = evt_group;
        gatt_arg->out_queue = queue_out;
        ret = xTaskCreate(
                gatt_task, 
                "gatt_task", 
                configMINIMAL_STACK_SIZE * 4,
                (void *) gatt_arg,
                5,
                &ws_task_handle);
        if (ret == pdFALSE) {
            ESP_LOGE(MAIN_TAG, "start: Error creando websocket_task");
            return -1;
        }
    }
    // Arrancar Task MEASURE
    meas_task_arg_t *meas_arg = (meas_task_arg_t *) calloc(1, sizeof(meas_task_arg_t));
    meas_arg->nodo_evt_group = evt_group;
    meas_arg->out_queue = queue_out;
    ret = xTaskCreate(
            measure_task, 
            "measure_task", 
            configMINIMAL_STACK_SIZE * 4,
            (void *) meas_arg,
            5,
            &meas_task_handle);
    if (ret == pdFALSE) {
        ESP_LOGE(MAIN_TAG, "%s: Error creando measure_task",__func__);
        return -1;
    }
    //TODO: Liberar token, task_arg en salida limpia
    return 0;
}

void start_notify_connected_cb(EventGroupHandle_t evt_group, bool connected) {
    if (connected == CONN_OK) {
        ESP_LOGI(MAIN_TAG, "start_notify_connected_cb: Conexión exitosa...");
        // Activar bit WIFI_OK_EV
        xEventGroupSetBits(evt_group, WIFI_OK);
        // Guardar SSID/PSK en NVS
    } else {
        ESP_LOGI(MAIN_TAG, "start_notify_connected_cb: Falló conexión. Credenciales incorrectas!");
        xEventGroupClearBits(evt_group, WIFI_OK);
    }
}

void sync_time() {
    ESP_LOGI(MAIN_TAG, "Estableciendo zona horaia (%s)...", CONFIG_TIMEZONE);
    setenv("TZ", CONFIG_TIMEZONE, 1);
    tzset();
    ESP_LOGI(MAIN_TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_init();
    const int retry_count = 40;
    int retry = 0;
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && retry < retry_count) {
        ESP_LOGI(MAIN_TAG, "Esperando a sincronizar el tiempo...");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}
