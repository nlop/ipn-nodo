#include "measure.h"
/* TODO:
 *      + Revisar la atenuación de los ADCs dependiendo de los máximos de entrada para cada
 *      dispositivo
 *      + Considerar utilizar op-amps para un mejor acoplamiento del sensor de humedad o aumentar
 *      la atenuación
 *
 */
void measure_task(void *pvParameters) {
    meas_task_arg_t *arg = (meas_task_arg_t *) pvParameters;
    const adc_bits_width_t width = ADC_WIDTH_BIT_12;
    const adc_atten_t atten = ADC_ATTEN_DB_0;
    const adc1_channel_t adc_lm35 = ADC1_CHANNEL_6;
    const adc1_channel_t adc_hum = ADC1_CHANNEL_7;
    esp_adc_cal_characteristics_t *adc1_ch6_chars, *adc1_ch7_chars;
    // Preparar dispositivos
    uint32_t adc_value, lm35_vol, hum_vol;
#if CONFIG_TSL2561_ENABLED 
    uint32_t lux;
    esp_err_t res;
    /** Configuración TSL2561 **/
    ESP_ERROR_CHECK(i2cdev_init());
    tsl2561_t dev;
    memset(&dev, 0, sizeof(tsl2561_t));
    ESP_ERROR_CHECK(tsl2561_init_desc(&dev, ADDR, 0, SDA_GPIO, SCL_GPIO));
    ESP_ERROR_CHECK(tsl2561_init(&dev));
    ESP_LOGI(MEAS_TAG, "Found TSL2561 in package %s\n", dev.package_type == TSL2561_PACKAGE_CS ? "CS" : "T/FN/CL");
#endif
    /*** Configuración ADC1 ***/
    adc1_config_width(width);
    // Canal 6 del ADC1 (GPIO34)
    adc1_config_channel_atten(adc_lm35, atten);
    // Canal 7 del ADC1 (GPIO35)
    adc1_config_channel_atten(adc_hum, ADC_ATTEN_DB_6);
    //Caracterizacion ADC -- Declarar la curva de conversión para el ADC
    // Canal 6
    adc1_ch6_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, atten, width, DEFAULT_VREF, adc1_ch6_chars);
    // Canal 7
    adc1_ch7_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_6, width, DEFAULT_VREF, adc1_ch7_chars);
    // Esperar a que se alce el evento HTTP_OK
    ESP_LOGI(MEAS_TAG, "Esperando a HTTP_OK, GATT_TASK_OK (COMM_DISPATCHER_OK) ...");
    xEventGroupWaitBits(arg->nodo_evt_group, COMM_DISPATCHER_OK, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(MEAS_TAG, "Empezando a medir!");
    ws_queue_msg_t ws_msg = {0};
    measure_vector_t mvector = {0};
    mvector.len = 3; 
    mvector.data = (measure_t *) calloc(mvector.len, sizeof(measure_t));
    ws_msg.type = MSG_MEAS_VECTOR;
    ws_msg.meas_vector = &mvector;
    for(;;) {
        adc_value = 0;
        // 1er Canal - LM35
        for(int i = 0; i < SAMPLES; i++) {
            adc_value += adc1_get_raw(adc_lm35);
        }
        adc_value /= SAMPLES;
        lm35_vol = esp_adc_cal_raw_to_voltage(adc_value, adc1_ch6_chars);
        mvector.data[0].type = TEMPERATURE;
        mvector.data[0].value = lm35_vol;
        ESP_LOGI(MEAS_TAG, " LM35 => RAW = %d, mV = %d", adc_value, lm35_vol);
        // 2do Canal - Humedad
        adc_value = 0;
        for(int i = 0; i < SAMPLES; i++) {
            adc_value += adc1_get_raw(adc_hum);
        }
        adc_value /= SAMPLES;
        hum_vol = esp_adc_cal_raw_to_voltage(adc_value, adc1_ch7_chars);
        ESP_LOGI(MEAS_TAG, " Humedad => RAW = %d, mV = %d", adc_value, hum_vol);
        mvector.data[1].type = HUMIDITY;
        mvector.data[1].value = hum_vol;
#if CONFIG_TSL2561_ENABLED 
        // TSL2561
        if ((res = tsl2561_read_lux(&dev, &lux)) != ESP_OK)
            ESP_LOGE(MEAS_TAG, "Error leyendo dispositivo");
        else
            ESP_LOGI(MEAS_TAG, "Lux: %u", lux);
        mvector.data[2].type = LIGHT;
        mvector.data[2].value = lux;
#else
        mvector.data[2].type = LIGHT;
        mvector.data[2].value = 0;
#endif
        ESP_LOGI(MEAS_TAG, "Enviando datos...");
        xQueueSendToBack(arg->out_queue, &ws_msg, portMAX_DELAY);
        ESP_LOGI(MEAS_TAG, "Datos enviados!");
        vTaskDelay(pdMS_TO_TICKS(MEASURE_RATE_MS));
    }
    vTaskDelete(NULL);
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
