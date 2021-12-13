#ifndef MEAUSRE_H
#define MEAUSRE_H

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp32/nodo_def.h"
#include "measure/measure_def.h"
#include "esp32/nodo_events.h"
#include "nodo_queue.h"
#include "nodo_bluetooth.h"
#include "gatt/gattc.h"
#include "utils/mac.h"
#include "storage/spiffs.h"
#include "storage/nodo_nvs.h"

#if CONFIG_TSL2561_ENABLED 
#include <tsl2561.h>
#define ADDR                TSL2561_I2C_ADDR_FLOAT
#endif

#if CONFIG_BH1750_ENABLED 
#include <bh1750.h>
#define ADDR                BH1750_ADDR_LO
#endif

#define MEAS_TAG            "NODO-MEASURE"
#define MEASURE_RATE_MS     CONFIG_MEASURE_RATE_MS
#define SAMPLES             CONFIG_SAMPLES
#define DEFAULT_VREF        CONFIG_VREF

// Pines para comunicación I2C
#if CONFIG_TSL2561_ENABLED || CONFIG_BH1750_ENABLED
// i2c pins
#define SDA_GPIO            18         // GPIO18 => PIN 9 (DER)
#define SCL_GPIO            19         // GPIO19 => PIN 8 (DER)
#define INIT_I2C_ATTEMPTS   10
#endif

/** Estructuras **/
typedef struct meas_task_arg_t {
    QueueHandle_t out_queue;            // Cola para recibir paquetes enviados por otros tasks
    QueueHandle_t ctrl_queue;
    EventGroupHandle_t nodo_evt_group;  // Handle al event group de todo el nodo. Usado para
                                        // esperar al evento HTTP_OK
    spiffs_db_t *gattc_db;
} meas_task_arg_t;

/** Funciones **/
void measure_task(void *pvParameters);

#endif 
