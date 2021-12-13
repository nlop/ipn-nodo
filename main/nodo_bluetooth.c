#include "nodo_bluetooth.h"
//#define SPP_CONG_MANAGE 1

/** SPP **/
#define SPP_SERVER_NAME "SPP_SERVER"
#define AP_REG 0xE
esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE; // Máscara de seguridad
esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE; // Rol (esclavo)

QueueHandle_t spp_queue_in, spp_queue_out;

/*
 * Punteros a funciones callback
 */
nodo_spp_recv_cb spp_recv_cb;

/*
 * Handle del task que maneja la escritura al nivel protocolo SPP *
 */
TaskHandle_t send_task_handle;
/**
 * Handle a la conexión bluetooth actual
 */
uint32_t bt_conn_handle;

esp_bt_controller_status_t nodo_bt_status(void) {
    return esp_bt_controller_get_status();
}

int nodo_bt_init(esp_bt_mode_t mode) {
    esp_err_t ret;
    /* 
     * Obtener la memoria para el controlador de bluettoth utilizando el modo BLE
     * -> Obtener en modo BLE, sí se libera en BT Classic puede generar problemas
     */
    //ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    // Obtener la configuración predefinida para el controlador BT
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    bt_cfg.mode = mode;

    // Inicialziar el controlador con la configuración obtenida
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(BT_TAG, "%s: Falla al inicializar el controlador BT: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }

    // Habilitar el controlador de bluetooth utilizando BT Classic
    if ((ret = esp_bt_controller_enable(mode)) != ESP_OK) {
        ESP_LOGE(BT_TAG, "%s: Falla habilitando el controlador BT: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }

    // Inicializar el stack de bluedroid (operador de la infraestructura BT)
    if ((ret = esp_bluedroid_init()) != ESP_OK) {
        ESP_LOGE(BT_TAG, "%s: initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }

    // Habilitar el stack
    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(BT_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }
    return 0;
}

/**
 * Inicializar un canal de comunicación serial mediante Bluetooth por
 * medio del perfil SPP
 *
 * Argumentos:
 *      recv_cb : Función que implementa el mecanismo de recepeción de datos al nivel protocolo
 * Retorna:
 *      Estructura de tipo spp_init_ret_t con los handles para las colas de entrada y salida así
 *      como el estado de salida de la función
 */

spp_init_ret_t nodo_bt_spp_init(nodo_spp_recv_cb recv_cb) {
    spp_init_ret_t init_ret = {0};
    /* Modo de operación de SPP */
    esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
    esp_err_t ret;
    /* 
     * Registrar una función callback para manejar los eventos GAP 
     * (ver definición de enum esp_bt_gap_cb_event_t)
     */
    if ((ret = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK) {
        ESP_LOGE(BT_TAG, "%s gap register failed: %s\n", __func__, esp_err_to_name(ret));
        init_ret.status = -1;
        return init_ret;
    }
    // Asignar función callback de lectura
    spp_recv_cb = recv_cb;
    /* 
     * Registrar una función callback para manejar los eventos SPP - serial. comms. 
     * (ver definición de enum esp_spp_cb_t) 
     */
    if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK) {
        ESP_LOGE(BT_TAG, "%s spp register failed: %s\n", __func__, esp_err_to_name(ret));
        init_ret.status = -1;
        return init_ret;
    }

    /* Inicializar el módulo SPP (comm. serial) */
    if ((ret = esp_spp_init(esp_spp_mode)) != ESP_OK) {
        ESP_LOGE(BT_TAG, "%s spp init failed: %s\n", __func__, esp_err_to_name(ret));
        init_ret.status = -1;
        return init_ret;
    }
    /*
     * Set default parameters for Legacy Pairing
     * Use variable pin, input pin code when pairing
     */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);
    // Lanzar tareas (tasks) y crear filas (queues)
    spp_queue_in = xQueueCreate(SPP_QUEUE_LEN, sizeof(spp_msg_t));
    spp_queue_out = xQueueCreate(SPP_QUEUE_LEN, sizeof(spp_msg_t));
    //*queue_in = &spp_queue_in;
    init_ret.in_queue = spp_queue_in;
    //*queue_out = &spp_queue_out;
    init_ret.out_queue = spp_queue_out;
    // Arrancar el Task que habilita la salida de datos (send)
    xTaskCreate(
            nodo_init_send_task, 
            "nodo_init_send_task", 
            configMINIMAL_STACK_SIZE * 8, 
            (void *) spp_queue_out,  
            5,
            &send_task_handle);
    //*send_handle = send_task_handle; 
    init_ret.send_handle = send_task_handle;
    return init_ret;
}

void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event) {
        case ESP_SPP_INIT_EVT:
            ESP_LOGI(BT_TAG, "ESP_SPP_INIT_EVT");
            // Empezar el servido en modo esclavo, utilizando el canal 0
            esp_spp_start_srv(sec_mask,role_slave, 0, SPP_SERVER_NAME);
            break;
        case ESP_SPP_DISCOVERY_COMP_EVT:
            ESP_LOGI(BT_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
            break;
        case ESP_SPP_OPEN_EVT:
            ESP_LOGI(BT_TAG, "ESP_SPP_OPEN_EVT");
            break;
        case ESP_SPP_CLOSE_EVT:
            ESP_LOGI(BT_TAG, "ESP_SPP_CLOSE_EVT");
            break;
        case ESP_SPP_START_EVT:
            ESP_LOGI(BT_TAG, "ESP_SPP_START_EVT");
            esp_bt_dev_set_device_name(BT_DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            break;
        case ESP_SPP_CL_INIT_EVT:
            ESP_LOGI(BT_TAG, "ESP_SPP_CL_INIT_EVT");
            break;
        case ESP_SPP_DATA_IND_EVT:
            ESP_LOGI(BT_TAG, "ESP_SPP_DATA_IND_EVT len=%d handle=%d",
                    param->data_ind.len, param->data_ind.handle);
            esp_log_buffer_hex(BT_TAG ,param->data_ind.data,param->data_ind.len);
            // Accionar el callback pasado a la función
            spp_recv_cb(spp_queue_in, param);
            break;
        case ESP_SPP_CONG_EVT:
            ESP_LOGI(BT_TAG, "ESP_SPP_CONG_EVT");
#if SPP_CONG_MANAGE
            /**
             * Desbloquear el semáforo que maneja la congestión hasta que ya no exista
             * congestión (cong)
             */
            if ( param->write.cong == false ) {
                ESP_LOGI(BT_TAG, "Desbloqueando semáforo...");
                xTaskNotifyGive(send_task_handle);
            }
#endif
            break;
        case ESP_SPP_WRITE_EVT:
            ESP_LOGI(BT_TAG, "ESP_SPP_WRITE_EVT");
#if SPP_CONG_MANAGE
            /**
             * Desbloquear el semáforo que maneja la congestión solamente si la escritura
             * no marcó congestión (cong)
             */
            if ( param->write.cong == false ) {
                ESP_LOGI(BT_TAG, "Desbloqueando semáforo...");
                xTaskNotifyGive(send_task_handle);
            }
#endif
            break;
        case ESP_SPP_SRV_OPEN_EVT:
            ESP_LOGI(BT_TAG, "ESP_SPP_SRV_OPEN_EVT");
            /**
             * Al realizar una conexión, asignar un valor al handle de la conexión
             */
            if (param->srv_open.status == ESP_SPP_SUCCESS) {
                ESP_LOGI(BT_TAG, "Conexión exitosa, asignando handle...");
                bt_conn_handle = param->srv_open.handle;
            }
            break;
        case ESP_SPP_SRV_STOP_EVT:
            ESP_LOGI(BT_TAG, "ESP_SPP_SRV_STOP_EVT");
            break;
        case ESP_SPP_UNINIT_EVT:
            ESP_LOGI(BT_TAG, "ESP_SPP_UNINIT_EVT");
            break;
        default:
            break;
    }
}

/**
 * Función de lectura (recv) para la inicialización (init) -- capa protocolo
 */
void nodo_spp_init_recv_cb(QueueHandle_t queue, esp_spp_cb_param_t *param) { 
    spp_msg_t msg = {0};
    if (param->data_ind.len > 0 ) {
        switch( param->data_ind.data[0] ) {
            case MSG_SSID:
                ESP_LOGI(BT_TAG, "MSG_SSID!");
                msg.type = MSG_SSID;
                msg.msg_ssid.ssid = copy_msg(param->data_ind.len - 1, param->data_ind.data + 1);
                msg.msg_ssid.len = param->data_ind.len - 1;
                break;
            case MSG_PSK:
                ESP_LOGI(BT_TAG, "MSG_PSK!");
                msg.type = MSG_PSK;
                if ( param->data_ind.len > 2 ) {
                    msg.msg_psk.psk = copy_msg(param->data_ind.len - 1, param->data_ind.data + 1);
                    msg.msg_psk.len = param->data_ind.len - 1;
                }
                break;
            case MSG_INIT:
                ESP_LOGI(BT_TAG, "MSG_INIT");
                msg.type = MSG_INIT;
                break;
            case MSG_INIT_BLE:
                ESP_LOGI(BT_TAG, "MSG_INIT_BLE");
                msg.type = MSG_INIT_BLE; 
                if ( param->data_ind.len > 2 ) {
                    msg.ble_init.instance_id = copy_msg(param->data_ind.len - 1, param->data_ind.data + 1);
                    msg.ble_init.len = param->data_ind.len - 1;
                }
                break;
            case MSG_TOKEN:
                ESP_LOGI(BT_TAG, "MSG_TOKEN");
                msg.type = MSG_TOKEN; 
                if ( param->data_ind.len > 2 ) {
                    msg.msg_token.token = copy_msg(param->data_ind.len - 1, param->data_ind.data + 1);
                    msg.msg_token.len = param->data_ind.len - 1;
                }
                break;
            default:
                msg.type = MSG_UNKNOWN;
        }
    }
    xQueueSendToBack(queue, &msg, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(500));
    //ESP_LOGI(BT_TAG, "Enviando a la fila...");
}

/* 
 * Alocar un arreglo de bytes de tamaño 'len', y copiar los contenidos de src
 * con a los más, 'len' bytes
 *
 */
uint8_t *copy_msg(uint8_t len, uint8_t *src) {
    uint8_t *tmp = calloc(len, sizeof(uint8_t)); 
    memcpy(tmp, src, len);
    tmp[len] = '\0';
    return tmp;
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT:{
                                          if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                                              ESP_LOGI(BT_TAG, "authentication success: %s", param->auth_cmpl.device_name);
                                              esp_log_buffer_hex(BT_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
                                          } else {
                                              ESP_LOGE(BT_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
                                          }
                                          break;
                                      }
        case ESP_BT_GAP_PIN_REQ_EVT:{
                                        ESP_LOGI(BT_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
                                        if (param->pin_req.min_16_digit) {
                                            ESP_LOGI(BT_TAG, "Input pin code: 0000 0000 0000 0000");
                                            esp_bt_pin_code_t pin_code = {0};
                                            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
                                        } else {
                                            ESP_LOGI(BT_TAG, "Input pin code: 1234");
                                            esp_bt_pin_code_t pin_code;
                                            pin_code[0] = '1';
                                            pin_code[1] = '2';
                                            pin_code[2] = '3';
                                            pin_code[3] = '4';
                                            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
                                        }
                                        break;
                                    }
        case ESP_BT_GAP_MODE_CHG_EVT:
                                    ESP_LOGI(BT_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%d", param->mode_chg.mode);
                                    break;
        default: {
                     ESP_LOGI(BT_TAG, "event: %d", event);
                     break;
                 }
    }
    return;
}

/*********** Función de escritura (send) para la inicialización (init) ***********
 * Esta función corre sobre un Task y es inicializada junto al stack genérico de SPP
 */
void nodo_init_send_task(void *pvParameters) {
    ESP_LOGI(BT_TAG, "Empezando nodo_init_read_task...");
    QueueHandle_t queue_out = (QueueHandle_t) pvParameters;
    //QueueHandle_t inQueue = (QueueHandle_t) pvParameters;
    spp_msg_t msg_buffer = {0};
#if SPP_CONG_MANAGE
    BaseType_t sem_take = pdTRUE;
    uint32_t sem_sta = 0;
#endif
    for(;;) {
        xQueueReceive(queue_out,  (void *) &msg_buffer, portMAX_DELAY);
        ESP_LOGI(BT_TAG, "Recibiendo de la fila...");
        /*
         * TaskNotify que actúa como semáforo binario para controlar la escritura (en caso
         * de presentar congestión)
         */
#if SPP_CONG_MANAGE
        ESP_LOGI(BT_TAG, "Esperando semáforo...");
        sem_sta = ulTaskNotifyTake(sem_take, portMAX_DELAY); // TODO: Ajustar delay
        if( sem_sta == 1 ) {
#endif
            switch( msg_buffer.type ) {
                case MSG_SCAN_OK:
                    ESP_LOGI(BT_TAG, "%s: MSG_SCAN_OK APs found: %d", __func__, msg_buffer.msg_scan_ok.ap_scanned);
                    uint8_t scanok_bytes[2] = {MSG_SCAN_OK, msg_buffer.msg_scan_ok.ap_scanned}; 
                    esp_spp_write(bt_conn_handle, 2 * sizeof(uint8_t), (uint8_t*) &scanok_bytes);
                    break;
                case MSG_SCAN_DONE:
                    ESP_LOGI(BT_TAG, "%s: MSG_SCAN_DONE", __func__);
                    uint8_t scan_done_bytes = MSG_SCAN_DONE;
                    esp_spp_write(bt_conn_handle, sizeof(uint8_t), (uint8_t*) &scan_done_bytes);
                    break;
                case MSG_SCAN_SEND:
                    ESP_LOGI(BT_TAG, "%s: MSG_SCAN_SEND", __func__);
                    /*
                     * Enviar cada una de las entradas de APs
                     */
                    for(uint8_t i = 0; i < msg_buffer.ap_results->len; i++) {
                        ESP_LOGI(BT_TAG, "Enviando AP #%02d", i);
                        byte_buffer_t ap_data = nodo_ap_data_prepare(&(msg_buffer.ap_results->results[i]));
                        esp_spp_write(bt_conn_handle, ap_data.len, ap_data.data);
#if SPP_CONG_MANAGE
                        sem_sta = ulTaskNotifyTake(sem_take, portMAX_DELAY); // TODO: Ajustar delay
#endif
                        /*
                         * Liberar buffer temporal de datos
                         */
                        free(ap_data.data);
                    }
                    /*
                     * Liberar memoria dinámica de los resultados de APs
                     */
                    free(msg_buffer.ap_results->results);
                    break;
                case MSG_WIFI_CONN_OK:
                    ESP_LOGI(BT_TAG, "%s: MSG_WIFI_CONN_OK", __func__);
                    esp_spp_write(bt_conn_handle, sizeof(uint8_t), (uint8_t*) &msg_buffer.type);
                    break;
                case MSG_WIFI_CONN_FAIL:
                    ESP_LOGI(BT_TAG, "%s: MSG_WIFI_CONN_FAIL", __func__);
                    esp_spp_write(bt_conn_handle, sizeof(uint8_t), (uint8_t*) &msg_buffer.type);
                    break;
                case MSG_TOKEN_ACK:
                    ESP_LOGI(BT_TAG, "%s: MSG_TOKEN_ACK", __func__);
                    esp_spp_write(bt_conn_handle, sizeof(uint8_t), (uint8_t*) &msg_buffer.type);
                    break;
                case MSG_SERV_CONN_OK:
                    ESP_LOGI(BT_TAG, "%s: MSG_SERV_CONN_OK", __func__);
                    esp_spp_write(bt_conn_handle, sizeof(uint8_t), (uint8_t*) &msg_buffer.type);
                    break;
                case MSG_SERV_CONN_FAIL:
                    ESP_LOGI(BT_TAG, "%s: MSG_SERV_CONN_FAIL", __func__);
                    esp_spp_write(bt_conn_handle, sizeof(uint8_t), (uint8_t*) &msg_buffer.type);
                    break;
                case MSG_GATT_OK:
                    ESP_LOGI(BT_TAG, "%s: MSG_GATT_OK", __func__);
                    esp_spp_write(bt_conn_handle, sizeof(uint8_t), (uint8_t*) &msg_buffer.type);
                    break;
               default:
                    break;
            }
#if SPP_CONG_MANAGE
        } else {
            ESP_LOGE(BT_TAG, "TaskNotify timeout");
        }
#endif
    }
    vTaskDelete(NULL);
}

byte_buffer_t nodo_ap_data_prepare(wifi_ap_record_t *ap_record) {
    uint8_t ssid_len = strlen( (char*) &(ap_record->ssid) );
    ESP_LOGI(BT_TAG, "ssid len : %d", ssid_len);
    /**
     *  Versión 1.0 (Funciona con el protitipo de APP)
     *  --------------------------------------------------
     * | RSSI |  AuthType | SSID len (int) | ... SSID ... |
     *  --------------------------------------------------
     *
     *  Versión 1.1 (candidato a version final)
     *  --------------------------------------------------
     * | AP_REG | RSSI |  AuthType | ... SSID ... | CL+RF |
     *  --------------------------------------------------
     */
    uint8_t data_len = 5 + ssid_len;
    ESP_LOGI(BT_TAG, "ap_data len : %d", data_len);
    uint8_t *ap_data = calloc(data_len, sizeof(uint8_t));
    ap_data[0] = AP_REG;
    ap_data[1] = ap_record->rssi;
    ap_data[2] = get_auth_mode(ap_record->authmode);
    /**
     * Copiar los carácteres del SSID
     */
    memcpy(ap_data + 3, ap_record->ssid, ssid_len);
    /**
     * Agregar CL+RF
     */
    ap_data[data_len - 2] = '\r';
    ap_data[data_len - 1] = '\n';
    byte_buffer_t ap_data_buff = { .len = data_len, .data = ap_data };
    return ap_data_buff;
}

/*
 * Desactivar el canal de comunicación SPP
 */
void nodo_spp_disable(void) {
    esp_err_t ret = esp_spp_stop_srv();
    if( ret != ESP_OK ) {
        ESP_LOGE(BT_TAG, "%s: Error cerrando servidor SPP [code %s]", __func__, esp_err_to_name(ret));
    }
    ret = esp_spp_deinit();
    if( ret != ESP_OK ) {
        ESP_LOGE(BT_TAG, "%s: Error desactivando SPP [code %s]", __func__, esp_err_to_name(ret));
    }
}

/*
 * Desactivar el stack Bluetooth SPP
 */
void nodo_bt_disable(void) {
    // Desactivar el stack Bluedroid
    esp_bluedroid_disable();
    // Desinicializar el stack Bluedroid
    esp_bluedroid_deinit();
    // Desactivar el controlador Bluetooth
    esp_bt_controller_disable();
    // Desinicializar el controlador Bluetooth
    esp_bt_controller_deinit();
    // Liberar los recursos de memoria del controlador
    /*
     * TODO: Revisar si la invocación de esta función causa problemas
     *en el proceso de inicialización
     */
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
}
