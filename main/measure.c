#include "measure.h"
/* TODO:
 *      + Revisar la atenuación de los ADCs dependiendo de los máximos de entrada para cada
 *      dispositivo
 *      + Considerar utilizar op-amps para un mejor acoplamiento del sensor de humedad o aumentar
 *      la atenuación
 *
 */


static esp_adc_cal_characteristics_t *adc1_ch6_chars, *adc1_ch7_chars;
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc1_channel_t adc_lm35 = ADC1_CHANNEL_6;
static const adc1_channel_t adc_hum = ADC1_CHANNEL_7;
static measure_t mvector_data[3];
static measure_vector_t mvector = { .len = 3, .data = mvector_data };
static ws_queue_msg_t ws_msg = { .meas_vector = &mvector };
#if CONFIG_TSL2561_ENABLED 
static tsl2561_t dev;
#elif CONFIG_BH1750_ENABLED 
static i2c_dev_t dev;
#endif

enum meas_type_t { INSTANT, NORMAL };

static int measure(enum meas_type_t type, QueueHandle_t out_queue);
static int sinknode_measure(spiffs_db_t *gattc_db);
static int init_measure_(void);

void measure_task(void *pvParameters) {
    static BaseType_t queue_ret;
    meas_task_arg_t *arg = (meas_task_arg_t *) pvParameters;
    init_measure_();
    // Esperar a que se alce el evento HTTP_OK
    ESP_LOGI(MEAS_TAG, "Esperando a HTTP_OK, GATT_TASK_OK (COMM_DISPATCHER_OK) ...");
    xEventGroupWaitBits(arg->nodo_evt_group, COMM_DISPATCHER_OK, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(MEAS_TAG, "Empezando a medir!");
    static ctrl_msg_t ctrl_msg = {0};
    //mvector.data = (measure_t *) calloc(mvector.len, sizeof(measure_t));
    mvector.dev_addr = nodo_get_mac();
    ws_msg.meas_vector = &mvector;
    /* Lectura inicial */
    measure(NORMAL, arg->out_queue);
    for(;;) {
        ESP_LOGI(MEAS_TAG, "Datos enviados!");
        if ( dev_type == NODO_WIFI || dev_type == SINKNODE ) {
            queue_ret = xQueueReceive(arg->ctrl_queue , &ctrl_msg ,pdMS_TO_TICKS(MEASURE_RATE_MS));
            if ( queue_ret == pdPASS ) {
                /* TODO: Lógica de mensajes */
                if ( ctrl_msg.type == MSG_MEASURE_INST ) {
                    if ( ctrl_msg.remote_addr == NULL ) {
                        measure(INSTANT, arg->out_queue);
                    } else {
                        // TODO: Revisar remote_addr guardado en gattc_db
                    }
                }
            } else {
                /* TODO: Si acaba MEASURE_RATE_MS y no hay msg, realizar mediciones */
                measure(NORMAL, arg->out_queue);
                if ( ( dev_type == SINKNODE ) && ( arg->gattc_db != NULL ) ) {
                    sinknode_measure(arg->gattc_db);
                }
            }
        } else {
            measure(NORMAL, arg->out_queue);
            vTaskDelay(pdMS_TO_TICKS(MEASURE_RATE_MS));
        }
    }
    vTaskDelete(NULL);
}

static int init_measure_(void) {
    // Preparar dispositivos
#if CONFIG_TSL2561_ENABLED || CONFIG_BH1750_ENABLED
    /* Inicializar el bus i2c */
    ESP_ERROR_CHECK(i2cdev_init());
#endif
#if CONFIG_TSL2561_ENABLED 
    ESP_LOGI(MEAS_TAG, "Utilizando sensor TSL2561!");
    memset(&dev, 0, sizeof(tsl2561_t));
    ESP_ERROR_CHECK(tsl2561_init_desc(&dev, ADDR, 0, SDA_GPIO, SCL_GPIO));
    ESP_ERROR_CHECK(tsl2561_init(&dev));
    ESP_LOGI(MEAS_TAG, "Found TSL2561 in package %s\n", dev.package_type == TSL2561_PACKAGE_CS ? "CS" : "T/FN/CL");
#elif CONFIG_BH1750_ENABLED 
    ESP_LOGI(MEAS_TAG, "Utilizando sensor BH1750!");
    memset(&dev, 0, sizeof(i2c_dev_t));
    ESP_ERROR_CHECK(bh1750_init_desc(&dev, ADDR, 0, SDA_GPIO, SCL_GPIO));
    ESP_ERROR_CHECK(bh1750_setup(&dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH));
#endif
    /*** Configuración ADC1 ***/
    adc1_config_width(width);
    // Canal 6 del ADC1 (GPIO34)
    adc1_config_channel_atten(adc_lm35, atten);
    // Canal 7 del ADC1 (GPIO35)
    adc1_config_channel_atten(adc_hum, ADC_ATTEN_DB_11);
    //Caracterizacion ADC -- Declarar la curva de conversión para el ADC
    // Canal 6
    adc1_ch6_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, atten, width, DEFAULT_VREF, adc1_ch6_chars);
    // Canal 7
    adc1_ch7_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, width, DEFAULT_VREF, adc1_ch7_chars);
    return 0;
}

static int measure(enum meas_type_t type, QueueHandle_t out_queue) {
    ws_msg.type = ( type ==  INSTANT ) ?  MSG_MEAS_VECTOR_INST : MSG_MEAS_VECTOR_NORM;
    static esp_err_t res;
    static uint32_t adc_value, lm35_vol, hum_vol;
    adc_value = 0;
    // 1er Canal - LM35
    for(int i = 0; i < SAMPLES; i++) {
        adc_value += adc1_get_raw(adc_lm35);
    }
    adc_value /= SAMPLES;
    lm35_vol = esp_adc_cal_raw_to_voltage(adc_value, adc1_ch6_chars);
    mvector_data[0].type = TEMPERATURE;
    mvector_data[0].value = lm35_vol;
    ESP_LOGI(MEAS_TAG, " LM35 => RAW = %d, mV = %d", adc_value, lm35_vol);
    // 2do Canal - Humedad
    adc_value = 0;
    for(int i = 0; i < SAMPLES; i++) {
        adc_value += adc1_get_raw(adc_hum);
    }
    adc_value /= SAMPLES;
    hum_vol = esp_adc_cal_raw_to_voltage(adc_value, adc1_ch7_chars);
    ESP_LOGI(MEAS_TAG, " Humedad => RAW = %d, mV = %d", adc_value, hum_vol);
    mvector_data[1].type = HUMIDITY;
    mvector_data[1].value = hum_vol;
#if CONFIG_TSL2561_ENABLED 
    static uint32_t lux;
    // TSL2561
    if ((res = tsl2561_read_lux(&dev, &lux)) != ESP_OK)
        ESP_LOGE(MEAS_TAG, "Error leyendo dispositivo");
    else
        ESP_LOGI(MEAS_TAG, "Lux: %u", lux);
    mvector_data[2].type = LIGHT;
    mvector_data[2].value = lux;
#elif CONFIG_BH1750_ENABLED
    static uint16_t lux;
    if ((res = bh1750_read(&dev, &lux)) != ESP_OK)
        ESP_LOGE(MEAS_TAG, "Error leyendo dispositivo");
    else
        ESP_LOGI(MEAS_TAG, "Lux: %u", lux);
    mvector_data[2].type = LIGHT;
    mvector_data[2].value = lux;
#else
    mvector_data[2].type = LIGHT;
    mvector_data[2].value = 0;
#endif
    xQueueSendToBack(out_queue, &ws_msg, portMAX_DELAY);
    return 0;
}

static int sinknode_measure(spiffs_db_t *gattc_db) {
    /* TODO: 
     *      - Revisar caso con n > 1 nodos (no se puede asegurar que el
     *      anterior nodo ya haya acabado cuando llega la siguiente
     *      iteración)
     */
    for(uint8_t i = 0; i < gattc_db->len; i++) {
        gattc_set_addr(gattc_db->data[i].raw_addr, 
                gattc_db->data[i].str_addr);
        ESP_LOGW(MEAS_TAG, "%s| Enviado datos de nodos...", __func__);
        for ( uint8_t i = 0; i < MAX_GATTC_ATTEMPTS; i++ ) {
            int ret = nodo_gattc_start();
            if ( ret != 0 ) {
                ESP_LOGE(MEAS_TAG, "%s GATTC reintento #%02d", __func__, i);
                vTaskDelay(pdMS_TO_TICKS(GATTC_WAIT_START_TIMEOUT));
            } else {
                break;
            }
        }
    }
    return 0;
}
/*
 * Función auxiliar para obtener la etiqueta de cada tipo de medición (utilizada para
 * generar entradas de JSON)
 */
char *get_measure_str(uint8_t type) {
    switch(type) {
        case TEMPERATURE:
            return "temperature";
        case HUMIDITY:
            return "humidity";
        case LIGHT:
            return "luminosity";
        case PH:
            return "ph";
        default:
            return NULL;
    }
}
