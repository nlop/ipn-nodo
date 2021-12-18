#ifndef NODO_GATTS_H
#define NODO_GATTS_H

#include <string.h>
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
#include "gatt/gatt_def.h"
#include "esp32/nodo_events.h"
#include "nodo_queue.h"

#define GATTS_TAG                   "BLE_GATTS"
#define GATT_TASK_TAG               "GATTS_TASK"
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
static const uint16_t temperature_uuid = TEMP_CHAR_UUID;        // SIG GATT Characteristics and Object Type
static const uint16_t humidity_uuid= HUMIDITY_CHAR_UUID; 
static const uint16_t lux_uuid = LUX_CHAR_UUID;
static const uint16_t ph_char_uuid = PH_CHAR_UUID; // pH
static const uint16_t system_id_uuid = TEST_DISCOVERY_UUID;
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
static const uint8_t default_system_id[] = { '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','1'};

/* Enum de atributos para el perfil */
enum {
    ID_SVC, 
    ID_INSTANCE_ID,
    ID_INSTANCE_ID_VAL,
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
static esp_gatts_attr_db_t gatt_db[DB_LEN] =  {
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
    [ID_INSTANCE_ID] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &char_declaration_uuid, 
                ESP_GATT_PERM_READ,
                CHAR_DECLARATION_SIZE,
                CHAR_DECLARATION_SIZE,
                (uint8_t *) &char_properties_rn 
            }},
    [ID_INSTANCE_ID_VAL] =
        {{ESP_GATT_AUTO_RSP},
            {
                ESP_UUID_LEN_16,
                (uint8_t *) &system_id_uuid, 
                ESP_GATT_PERM_READ,
                16*sizeof(uint8_t),
                16*sizeof(uint8_t),
                (uint8_t *) default_system_id 
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
                sizeof(int16_t),
                sizeof(int16_t),
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
                (uint8_t *) &ph_char_uuid, 
                ESP_GATT_PERM_READ,
                sizeof(int32_t),
                sizeof(int32_t),
                (uint8_t *) &init_value 
            }}
};



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

typedef struct gatts_instance_id_t {
    uint8_t *instance_id;
    size_t len;
} gatts_instance_id_t;

typedef struct gatt_task_arg_t {
    QueueHandle_t out_queue;            // Cola para recibir paquetes enviados por otros tasks
    EventGroupHandle_t nodo_evt_group;  // Handle al event group de todo el nodo.
} gatt_task_arg_t;

/* Funciones */
void init_gatt_server(const EventGroupHandle_t evt_group, gatts_instance_id_t *instance_id);
void gatt_task(void *pvParameters);

#endif
