#include "measure/measure.h"

enum meas_type_t { INSTANT, NORMAL };

/** Funciones locales **/
static int measure(enum meas_type_t type, QueueHandle_t out_queue);
static int sinknode_measure(spiffs_db_t *gattc_db);
static int init_measure_(void);
static void control(ctrl_msg_t msg, meas_task_arg_t *ctx);
static void gattc_init_callback(nodo_gattc_events_t evt, void *arg);
static int dev_discovery(uint8_t *addr, uint8_t *instance_id, QueueHandle_t queue);

static esp_adc_cal_characteristics_t *adc1_ch6_chars, *adc1_ch7_chars;
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc1_channel_t adc_lm35 = ADC1_CHANNEL_6;
static const adc1_channel_t adc_hum = ADC1_CHANNEL_7;
static measure_t mvector_data[3];
static measure_vector_t mvector = { .len = 3, .data = mvector_data };
static ws_meas_vector_t ws_mvector = { .measure = &mvector };
static ws_queue_msg_t ws_msg = { .meas_vector = &ws_mvector  };
#if CONFIG_TSL2561_ENABLED || CONFIG_BH1750_ENABLED
static bool lux_meas_ok = false;
#endif
#if CONFIG_TSL2561_ENABLED 
static tsl2561_t dev;
#elif CONFIG_BH1750_ENABLED 
static i2c_dev_t dev;
#endif

extern dev_type_t dev_type;
extern spiffs_db_t gattc_db;

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
    ws_mvector.dev_addr = nodo_get_mac();
    /* Lectura inicial */
    measure(NORMAL, arg->out_queue);
    if ( ( dev_type == SINKNODE ) && ( arg->gattc_db != NULL ) ) {
        sinknode_measure(arg->gattc_db);
    }
    for(;;) {
        ESP_LOGI(MEAS_TAG, "Datos enviados!");
        if ( dev_type == NODO_WIFI || dev_type == SINKNODE ) {
            queue_ret = xQueueReceive(arg->ctrl_queue , &ctrl_msg ,pdMS_TO_TICKS(MEASURE_RATE_MS));
            if ( queue_ret == pdPASS ) {
                /* TODO: Lógica de mensajes */
                control(ctrl_msg, arg);
            } else {
                /* TODO: Si acaba MEASURE_RATE_MS y no hay msg, realizar mediciones */
                measure(NORMAL, arg->out_queue);
                if ( ( dev_type == SINKNODE ) && ( arg->gattc_db != NULL ) ) {
                    sinknode_measure(arg->gattc_db);
                }
            }
        } else {
            measure(NORMAL, arg->out_queue);
            /* Realizar más lecturas cuando es BLE */
            if ( dev_type == NODO_BLE )  
                vTaskDelay(pdMS_TO_TICKS(MEASURE_RATE_MS / 4));
            else 
                vTaskDelay(pdMS_TO_TICKS(MEASURE_RATE_MS));
        }
    }
    vTaskDelete(NULL);
}

static int init_measure_(void) {
    // Preparar dispositivos
    for (int i = 0; i < INIT_I2C_ATTEMPTS; i++ ) {
#if CONFIG_TSL2561_ENABLED || CONFIG_BH1750_ENABLED
        /* Inicializar el bus i2c */
        if ( i2cdev_init() != ESP_OK ) {
            ESP_LOGE(MEAS_TAG, "%s Error iniciando i2c!", __func__);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        };
#endif
#if CONFIG_TSL2561_ENABLED 
        ESP_LOGI(MEAS_TAG, "Utilizando sensor TSL2561!");
        memset(&dev, 0, sizeof(tsl2561_t));
        if( tsl2561_init_desc(&dev, ADDR, 0, SDA_GPIO, SCL_GPIO) != ESP_OK ) {
            ESP_LOGE(MEAS_TAG, "%s Error iniciando TSL2561 (desc)!", __func__);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if( tsl2561_init(&dev) != ESP_OK ) {
            ESP_LOGE(MEAS_TAG, "%s Error iniciando TSL2561 (init)!", __func__);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        ESP_LOGI(MEAS_TAG, "Found TSL2561 in package %s\n", dev.package_type == TSL2561_PACKAGE_CS ? "CS" : "T/FN/CL");
#elif CONFIG_BH1750_ENABLED 
        ESP_LOGI(MEAS_TAG, "Utilizando sensor BH1750!");
        memset(&dev, 0, sizeof(i2c_dev_t));
        ESP_ERROR_CHECK(bh1750_init_desc(&dev, ADDR, 0, SDA_GPIO, SCL_GPIO));
        if( bh1750_init_desc(&dev, ADDR, 0, SDA_GPIO, SCL_GPIO) != ESP_OK ) {
            ESP_LOGE(MEAS_TAG, "%s Error iniciando TSL2561 (init)!", __func__);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if( bh1750_setup(&dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH) != ESP_OK) {
            ESP_LOGE(MEAS_TAG, "%s Error iniciando BH1750 (setup)!", __func__);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
#endif
        lux_meas_ok = true;
        break;
    }
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

static void control(ctrl_msg_t msg, meas_task_arg_t *ctx) {
    if ( msg.type == MSG_MEASURE_INST ) {
        if ( strcmp(msg.measure.dev_addr, nodo_get_mac()) == 0 ) {
            measure(INSTANT, ctx->out_queue);
        } else {
            // TODO: Revisar remote_addr guardado en gattc_db
            ESP_LOGW(MEAS_TAG, "TODO: Lecturas GATT");
        }
    } else if (msg.type == MSG_MEASURE_PH) {
        // TODO: Habilitar y leer el sensor de pH
        ESP_LOGW(MEAS_TAG, "%s| Sensor de pH (TODO)!", __func__);
    } else if (msg.type == MSG_DEV_DISCOVERY) {
        dev_discovery(msg.discovery.dev_addr, msg.discovery.instance_id, ctx->out_queue);
    } else {
        ESP_LOGE(MEAS_TAG, "%s| Tipo de mensaje desconocido!", __func__);
    }
}

static int dev_discovery(uint8_t *addr, uint8_t *instance_id, QueueHandle_t queue) {
    /* GATTC Init */
    uint16_t const init_char[] = { TEST_DISCOVERY_UUID };
    /* Habilitar el stack BT/BLE sí no esta habilitado */
    if (dev_type == NODO_WIFI && nodo_bt_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        //ESP_LOGI(MEAS_TAG, "%s| Debería habilitar BT/BLE & iniciar GATTC...", __func__);
        ESP_LOGI(MEAS_TAG, "%s: Habilitando stack BT/BLE...", __func__);
        if ( nodo_bt_init(ESP_BT_MODE_BLE) != 0 ) {
            ESP_LOGE(MEAS_TAG, "%s: Error iniciando stack Bluetooth!", __func__);
            return -1;
        }
        if (init_gatt_client(queue) != 0) {
            ESP_LOGE(MEAS_TAG, "%s: Error levantando cliente GATT!", __func__); 
            return -1;
        }
    }
    register_discovery_cb(gattc_init_callback);
    // TODO: Agregar str_addr para DISCOVERY_CMPL/FAIL
    char *addr_str = (char *) calloc(MAC_STR_LEN + 1, sizeof(char));
    get_mac_str(addr, addr_str);
    gattc_set_addr(addr, addr_str);
    gattc_set_instance_id(instance_id);
    if ( gattc_set_chars(init_char, sizeof(init_char)/sizeof(init_char[0])) != 0) {
        ESP_LOGE(MEAS_TAG, "%s: Error declarando chars!", __func__); 
        return -1; 
    }
    for ( uint8_t i = 0; i < MAX_GATTC_ATTEMPTS; i++ ) {
        int ret = nodo_gattc_start();
        if ( ret != 0 ) {
            ESP_LOGE(MEAS_TAG, "%s GATTC reintento #%02d", __func__, i);
            vTaskDelay(pdMS_TO_TICKS(GATTC_WAIT_START_TIMEOUT));
        } else {
            break;
        }
        if ( i == MAX_GATTC_ATTEMPTS - 1 && ret != 0 ) {
            ESP_LOGE(MEAS_TAG, "%s| Error iniciando cliente GATT", __func__);
            return -1;
        }
    }
    free(addr_str);
    return 0;
}

/* Callback enviado al proceso de inicialización de BLE */
static void gattc_init_callback(nodo_gattc_events_t evt, void *arg) {
    /* TODO: Emitir mensaje donde falla el proceso de descubrimiento
     * |-> Parcialmente cubierto en nodo_gattc.h! */
    if ( evt == DISCOVERY_CMPL ) {
        /* Guardar en SPIFFS */
        // ====>
        if ( ( dev_type == NODO_WIFI ) &&  ( spiffs_init() != 0 ) ) {
            ESP_LOGE(MEAS_TAG, "%s| Error inicializando almacenamiento SPIFFS!", __func__);
            return;
        }
        if ( ( spiffs_mounted() != 0 ) && ( spiffs_mount() != 0 ) ) {
            ESP_LOGE(MEAS_TAG, "%s| Error montando SPIFFS!", __func__);
            return;
        }
        if (spiffs_add_entry( (uint8_t *) arg ) != 0 ) {
            ESP_LOGE(MEAS_TAG, "%s| Error guardando entrada!", __func__);
            return;
        }
        if ( ( dev_type == NODO_WIFI ) && ( nvs_set_mode(SINKNODE) != 0 ) ) {
            ESP_LOGE(MEAS_TAG, "%s| Error guardando nuevo modo (SINKNODE)!", __func__);
        }
        spiffs_umount();
        // <====
        vTaskDelay(pdMS_TO_TICKS(5000));
        /* Reiniciar para inicializar con nuevo modo o leer de nuevo la lista y
         * realizar la primera lectura */
        ESP_LOGW(MEAS_TAG, "Reiniciando en 5s...!");
        esp_restart();
        /* Ruta alternativa sin reinicio <<< Genera errores de ejecución >>> */
        //gattc_db.len += 1;
        //db_entry_t *ret = reallocarray(gattc_db.data, gattc_db.len, sizeof(db_entry_t));
        //if ( ret == NULL )
        //    ESP_LOGE(MEAS_TAG, "%s| Error modificando gattc_db!", __func__);
        //else
        //    gattc_db.data = ret;
        ///* Rellenar datos de la última estación agregada */
        //uint8_t *tmp = (uint8_t *) calloc(MAC_ADDR_LEN, sizeof(uint8_t));
        //if ( tmp != NULL ) {
        //    memcpy(tmp, arg, MAC_ADDR_LEN);
        //} else {
        //    ESP_LOGE(MEAS_TAG, "%s| Error alojando memoria para raw_addr!", __func__);
        //    return;
        //}
        //gattc_db.data[gattc_db.len - 1].raw_addr = tmp;
        //char *tmp_str = (char *) calloc(MAC_STR_LEN + 1, sizeof(char));
        //get_mac_str(gattc_db.data[gattc_db.len - 1].raw_addr, tmp_str);
        //gattc_db.data[gattc_db.len - 1].str_addr = tmp_str;
        ///* Establecer los parámetros de lectura para GATTC */
        //if ( gattc_set_chars(chars, sizeof(chars)/sizeof(chars[0])) != 0) {
        //    ESP_LOGE(MEAS_TAG, "%s: Error declarando chars!", __func__); 
        //    return; 
        //}
    }
    unregister_discovery_cb();
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
    if ( lux_meas_ok == true ) {
        static uint32_t lux;
        // TSL2561
        if ((res = tsl2561_read_lux(&dev, &lux)) != ESP_OK)
            ESP_LOGE(MEAS_TAG, "Error leyendo dispositivo");
        else
            ESP_LOGI(MEAS_TAG, "Lux: %u", lux);
        mvector_data[2].type = LIGHT;
        mvector_data[2].value = lux;
    }
#elif CONFIG_BH1750_ENABLED
    if ( lux_meas_ok == true ) {
        static uint16_t lux;
        if ((res = bh1750_read(&dev, &lux)) != ESP_OK)
            ESP_LOGE(MEAS_TAG, "Error leyendo dispositivo");
        else
            ESP_LOGI(MEAS_TAG, "Lux: %u", lux);
        mvector_data[2].type = LIGHT;
        mvector_data[2].value = lux;
    }
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
