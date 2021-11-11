#ifndef NODO_GATTS_H
#define NODO_GATTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include <string.h>
#include "nodo_events.h"
#include "nodo_queue.h"

#define GATTS_TAG                   "BLE_GATTS"
#define GATT_TASK_TAG               "GATTS TASK"
#define GATTS_APP_ID                0x91                // ID escogido arbitrariamente
#define PROFILE_NUM                 1
#define NODO_SERVICE_ID             0
#define PROFILE_APP_IDX             0
#define NODO_DEV_NAME               "IPN-NODO"
#define SVC_INST_ID                 0
#define CHAR_DECLARATION_SIZE       (sizeof(uint8_t))
#define ADV_CONFIG_FLAG             (0x01 << 0)
#define SCAN_RSP_CONFIG_FLAG        (0x01 << 1)
#define MTU_SIZE                    500
#define DEV_BLE_APPEARANCE          0x556               // SIG Appearance Values (Multisensor)

/* Definición del servicio */

#define MAX_SERVICE_UUID_LEN        16
#define PRES_FMT_LEN                7
static uint8_t service_uuid[16] = {
    // xxxxxxxx-0000-1000-8000-00805F9B34FB <-- Direción base
    //first uuid, 16bit, [12],[13] is the value
    /* LSB <--------------------------------------------------------------------------------> MSB */
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x31, 0x39, 0x00, 0x00
};
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
/* Valores asignados por Bluetooth SIG */
static const uint16_t temperature_uuid = 0x2A6E;        // SIG GATT Characteristics and Object Type
static const uint16_t humidity_uuid= 0x2A6F; 
static const uint16_t lux_uuid = 0x2AFB;
static const uint16_t generic_uuid = 0x2AF9;
/* Characteristics formats (<<< LSB ----- MSB >>>) */
static const uint8_t temp_presentation_format[7] =  {0x10, -1, 0x2f, 0x27, 0x01, 0x00, 0x00};
static const uint8_t hum_presentation_format[7] =   {0x10, -3, 0x28, 0x27, 0x01, 0x00, 0x00};
static const uint8_t lum_presentation_format[7] =   {0x10,  0, 0x31, 0x27, 0x01, 0x00, 0x00};
/* Characteristics types UUIDs */
static const uint16_t char_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t char_pres_fmt_uuid = ESP_GATT_UUID_CHAR_PRESENT_FORMAT;
/* Characteristics props */
static const uint8_t char_properties_rn = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
/* Characteristics values */
static const int32_t init_value = 0;

/* Enum de atributos para el perfil */
enum {
    ID_SVC, 
    ID_CHAR_TEMP,
    ID_CHAR_TEMP_VAL,
    ID_CHAR_TEMP_FMT,
    ID_CHAR_HUM,
    ID_CHAR_HUM_VAL,
    ID_CHAR_HUM_FMT,
    ID_CHAR_LUM,
    ID_CHAR_LUM_VAL,
    ID_CHAR_LUM_FMT,
    ID_CHAR_PH,
    ID_CHAR_PH_VAL,
    DB_LEN};

/* Tabla de servicios Bluetooth LE */
static const esp_gatts_attr_db_t gatt_db[DB_LEN] =  {
    [ID_SVC] = 
        {{ESP_GATT_AUTO_RSP}, 
            {
                ESP_UUID_LEN_16,                        // Long. UUID
                (uint8_t *) &primary_service_uuid,      // Valor UUID
                ESP_GATT_PERM_READ,                     // Permisos entrada
                MAX_SERVICE_UUID_LEN,                   // Long. Max. entrada
                MAX_SERVICE_UUID_LEN,                   // Long. entrada
                (uint8_t *) service_uuid                // Valor entrada
            }},
    [ID_CHAR_TEMP] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &char_declaration_uuid, 
                ESP_GATT_PERM_READ,
                CHAR_DECLARATION_SIZE,
                CHAR_DECLARATION_SIZE,
                (uint8_t *) &char_properties_rn 
            }},
    [ID_CHAR_TEMP_VAL] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &temperature_uuid, 
                ESP_GATT_PERM_READ,
                sizeof(int32_t),
                sizeof(int32_t),
                (uint8_t *) &init_value 
            }},
    [ID_CHAR_TEMP_FMT] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &char_pres_fmt_uuid, 
                ESP_GATT_PERM_READ,
                PRES_FMT_LEN,
                PRES_FMT_LEN,
                (uint8_t *) &temp_presentation_format 
            }},
    [ID_CHAR_HUM] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &char_declaration_uuid, 
                ESP_GATT_PERM_READ,
                CHAR_DECLARATION_SIZE,
                CHAR_DECLARATION_SIZE,
                (uint8_t *) &char_properties_rn 
            }},
    [ID_CHAR_HUM_VAL] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &humidity_uuid, 
                ESP_GATT_PERM_READ,
                sizeof(int32_t),
                sizeof(int32_t),
                (uint8_t *) &init_value 
            }},
    [ID_CHAR_HUM_FMT] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &char_pres_fmt_uuid, 
                ESP_GATT_PERM_READ,
                PRES_FMT_LEN,
                PRES_FMT_LEN,
                (uint8_t *) &hum_presentation_format 
            }},
    [ID_CHAR_LUM] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &char_declaration_uuid, 
                ESP_GATT_PERM_READ,
                CHAR_DECLARATION_SIZE,
                CHAR_DECLARATION_SIZE,
                (uint8_t *) &char_properties_rn 
            }},
    [ID_CHAR_LUM_VAL] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &lux_uuid, 
                ESP_GATT_PERM_READ,
                sizeof(int32_t),
                sizeof(int32_t),
                (uint8_t *) &init_value 
            }},
    [ID_CHAR_LUM_FMT] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &char_pres_fmt_uuid, 
                ESP_GATT_PERM_READ,
                PRES_FMT_LEN,
                PRES_FMT_LEN,
                (uint8_t *) &lum_presentation_format 
            }},
    [ID_CHAR_PH] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &char_declaration_uuid, 
                ESP_GATT_PERM_READ,
                CHAR_DECLARATION_SIZE,
                CHAR_DECLARATION_SIZE,
                (uint8_t *) &char_properties_rn 
            }},
    [ID_CHAR_PH_VAL] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &generic_uuid, 
                ESP_GATT_PERM_READ,
                sizeof(int32_t),
                sizeof(int32_t),
                (uint8_t *) &init_value 
            }}
};

/* The length of adv data must be less than 31 bytes */
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval        = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance          = DEV_BLE_APPEARANCE,
    .manufacturer_len    = 0,    //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //test_manufacturer,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(service_uuid),
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// scan response data
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp        = true,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = DEV_BLE_APPEARANCE,
    .manufacturer_len    = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //&test_manufacturer[0],
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(service_uuid),
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* Funciones */
void init_gatt_server(const EventGroupHandle_t evt_group);
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void gatt_task(void *pvParameters);

/* Estructuras */
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

typedef struct gatt_task_arg_t {
    QueueHandle_t out_queue;            // Cola para recibir paquetes enviados por otros tasks
    EventGroupHandle_t nodo_evt_group;  // Handle al event group de todo el nodo.
} gatt_task_arg_t;

#endif
