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
#include "nodo_gatts.h"
#include "nodo_gattc.h"
#include "nodo_spiffs.h"

#define MAIN_TAG "MAIN"
#define GATT_MODE   true
enum conn_type_t { CONN_TYPE_GATT, CONN_TYPE_WEBSOCKET };

// Declaraciones
int start(dev_type_t dev_type, const EventGroupHandle_t evt_group);
void start_notify_connected_cb(EventGroupHandle_t evt_group, bool connected);
void sync_time();
int load_gattc_db(void);

/* temp */
const static uint16_t chars[3] = { TEMP_CHAR_UUID, HUMIDITY_CHAR_UUID, LUX_CHAR_UUID};
const static uint8_t child_addr [] = {0xec, 0x94, 0xcb, 0x4d, 0xac, 0x76};

/** Variables globales **/
db_entries_t gattc_db = {0};

dev_type_t dev_type = 0;            /* Tipo de dispositivo que esta corriendo (BLE, WIFI, WIFI + BLE) */

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
    /* Crear EventGroup para eventos del nodo */
    EventGroupHandle_t nodo_evt_group = xEventGroupCreate();
    if (nodo_evt_group == NULL) {
        ESP_LOGE(MAIN_TAG, "app_main: Error creando event group");
        return;
    }
    /* Buscar el tipo de dispositivo */
    uint8_t nvs_dev_type = nvs_get_mode();
    dev_type = nvs_dev_type;
    switch (dev_type) {
        case NODO_WIFI:
            ESP_LOGI(MAIN_TAG, "%s| Iniciando NODO_WIFI", __func__);
            break;
        case NODO_BLE:
            ESP_LOGI(MAIN_TAG, "%s| Iniciando NODO_BLE", __func__);
            break;
        case SINKNODE:
            ESP_LOGI(MAIN_TAG, "%s| Iniciando SINKNODE", __func__);
    }
    if (dev_type == NODO_WIFI || dev_type == SINKNODE) {
        /*
         * Buscar en la NVS las llaves NVS_AP_PART.AP_SSID/AP_PSK para saber si la
         * estación ha sido inicializada anteriormente
         */
        uint8_t *ssid = NULL;
        uint8_t *psk = NULL;
        if ( nvs_get_wifi_credentials(&ssid, &psk) == 0 ) {
            ESP_LOGI(MAIN_TAG, "Credenciales guardadas %s : %s", ssid, psk);
            ESP_LOGI(MAIN_TAG, "Inicializando estación: normal boot");
            nodo_wifi_init(start_notify_connected_cb, nodo_evt_group);
            nodo_wifi_set_credentials(ssid, psk);
            /* Sincronizar el tiempo con NTP */
            sync_time();
        } else {
            ESP_LOGI(MAIN_TAG, "No se ha configurado!\nInicializando estación: first boot...");
            nodo_init_dev(nodo_evt_group);
        }
    } else if (dev_type == NODO_BLE) {
        nodo_init_ble_gatts(nodo_evt_group); 
    } else {
        ESP_LOGI(MAIN_TAG, "No se ha configurado!");
        ESP_LOGI(MAIN_TAG, "Inicializando estación: first boot...");
        nodo_init_dev(nodo_evt_group);
        /* Leer nuevamente el tipo de dispositivo */
        if ( ( dev_type = nvs_get_mode() ) == 0) {
            ESP_LOGD(MAIN_TAG, "%s: Error leyendo DEV_TYPE en iniciación!", __func__);
            return;
        }
    }
    ESP_LOGI(MAIN_TAG, "%s: Arrancando tareas...", __func__);
    start(dev_type, nodo_evt_group);
}

/*
 * Arrancar las tareas normales de envío de datos y medición
 *
 * Argumentos:
 *      evt_group: Handle para el grupo de eventos que marca eventos relevantes
 *      del nodo
 */
int start(dev_type_t dev_type, const EventGroupHandle_t evt_group) {
    QueueHandle_t queue_out = 0;
    //QueueHandle_t queue_out;
    TaskHandle_t ws_task_handle, meas_task_handle;
    BaseType_t ret;
    if (dev_type == NODO_WIFI || dev_type == SINKNODE) {
        ESP_LOGI(MAIN_TAG, "Arrancado task WebSocket...");
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
                configMINIMAL_STACK_SIZE * 8,
                (void *) ws_arg,
                5,
                &ws_task_handle);
        if (ret == pdFALSE) {
            ESP_LOGE(MAIN_TAG, "%s: Error creando websocket_task", __func__);
            return -1;
        }
        if (dev_type == SINKNODE) {
            if ( load_gattc_db() != 0 ) {
                ESP_LOGE(MAIN_TAG, "%s| Error cargando GATTC DB!", __func__);
            }
            /* temp: Arrancar stack BT */ 
            if ( nodo_bt_init(ESP_BT_MODE_BLE) != 0 ) {
                ESP_LOGE(MAIN_TAG, "%s: Error iniciando stack Bluetooth!", __func__);
                return -1;
            }
            if (init_gatt_client(queue_out) != 0) {
                ESP_LOGE(MAIN_TAG, "%s: Error levantando cliente GATT!", __func__); 
                return -1;
            }
            gattc_set_addr(child_addr);
            if ( gattc_set_chars(chars, sizeof(chars)/sizeof(chars[0])) != 0) {
                ESP_LOGE(MAIN_TAG, "%s: Error declarando chars!", __func__); 
                return -1; 
            }
            /*
             * TODO:
             *  + Arrancar BLE
             *  + Arrancar GATTC peero:
             *      * Correr la rutina hasta la búsqueda
             *      * Utilizar callbacks para rastrear el avance del proceso de
             *      inicio
             *      * Separar la funcionalidad del código actual para trabajar
             *      en los casos de inicialización y monitoreo normal
             */
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
            configMINIMAL_STACK_SIZE * 6,
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
        // Activar bit WIFI_OK (COMM_CHANNEL_OK)
        xEventGroupSetBits(evt_group, COMM_CHANNEL_OK);
    } else {
        ESP_LOGI(MAIN_TAG, "start_notify_connected_cb: Falló conexión. Credenciales incorrectas!");
        xEventGroupClearBits(evt_group, COMM_CHANNEL_OK);
    }
}

int load_gattc_db(void) {
    if ( ( spiffs_mounted() != 0 ) && ( spiffs_mount() != 0 ) ) {
        ESP_LOGE(WSTASK_TAG, "%s| Error montando SPIFFS!", __func__);
        return -1;
    } else {
        int ret = spiffs_get_all(&gattc_db);
        if ( ret != 0 ) {
            ESP_LOGE(MAIN_TAG, "%s| Error leyendo GATTC_DB!", __func__);
            return -1;
        }
        for ( uint8_t i = 0; i < gattc_db.len; i++) {
            ESP_LOGI(MAIN_TAG, "DEV #%d", i);
            ESP_LOG_BUFFER_HEX(MAIN_TAG, gattc_db.data[i], ENTRY_LEN);
        }
        spiffs_umount();
    }
    return 0;
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
