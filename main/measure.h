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
#include "nodo_def.h"
#include "nodo_events.h"
#include "nodo_queue.h"
#include "nodo_gattc.h"
#include "nodo_mac.h"
#include "nodo_spiffs.h"

#if CONFIG_TSL2561_ENABLED 
#include <tsl2561.h>
#endif

#define MEAS_TAG            "NODO-MEASURE"
#define MEASURE_RATE_MS     CONFIG_MEASURE_RATE_MS
#define SAMPLES             CONFIG_SAMPLES
#define DEFAULT_VREF        CONFIG_VREF
#define MAX_GATTC_ATTEMPTS  15

#if CONFIG_TSL2561_ENABLED 
// TSL2561
// Pines para comunicación I2C
#define SDA_GPIO            18         // GPIO18 => PIN 9 (DER)
#define SCL_GPIO            19         // GPIO19 => PIN 8 (DER)
#define ADDR                TSL2561_I2C_ADDR_FLOAT
#endif

/** Estructuras **/
typedef struct meas_task_arg_t {
    QueueHandle_t out_queue;            // Cola para recibir paquetes enviados por otros tasks
    EventGroupHandle_t nodo_evt_group;  // Handle al event group de todo el nodo. Usado para
                                        // esperar al evento HTTP_OK
    spiffs_db_t *gattc_db;
} meas_task_arg_t;

/** Funciones **/
void measure_task(void *pvParameters);
char *get_measure_str(uint8_t type);

extern dev_type_t dev_type;

#endif 
