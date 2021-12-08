#include "gatt/gatts.h"

/** Funciones locales **/
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

/* Campo de control para rastrear la configuración del Advertisement y
 * Response. Dado que al registrar las dos configuraciones, se lanza un evento
 * que es aceptado por el event handler de GAP, se tiene que saber cuando
 * alguno de estos dos eventos activó esta bandera y consecuentemente, empezó
 * el proceso de Advertisement
 * 
 */
static uint8_t adv_config_done = 0;

/*
 * Arreglo de handles para todas las entradas de la tabla del servicio
 */
static uint16_t nodo_service_handles[DB_LEN];
/* Variable que contiene el instance_id del servicio, usada sí se pasa ese
 * agumento a 'init_gatt_server()'
 */
static uint8_t instance_id_value[16];

static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
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

/*
 * Instancia del servicio GATT, al lanzar el evento ESP_GATTS_REG_EVT se asigna
 * la interfaz del servicio
 */
static struct gatts_profile_inst nodo_service_profile_tab[PROFILE_NUM] = {
    [NODO_SERVICE_ID] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Inicializar con ESP_GATT_IF_NONE porque aún no hay interface. */
    }
};

/* Handle para el grupo de eventos del nodo */
static EventGroupHandle_t nodo_evt_group_handle;

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~ADV_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            /* advertising start complete event to indicate advertising start successfully or failed */
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TAG, "advertising start failed");
            }else{
                ESP_LOGI(GATTS_TAG, "advertising start successfully");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TAG, "Advertising stop failed");
            }
            else {
                ESP_LOGI(GATTS_TAG, "Stop adv successfully\n");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(GATTS_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
            break;
        default:
            break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT:{
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(NODO_DEV_NAME);
            if (set_dev_name_ret){
                ESP_LOGE(GATTS_TAG, "set device name failed, error code = %x", set_dev_name_ret);
            }
            //config adv data
            esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
            if (ret){
                ESP_LOGE(GATTS_TAG, "config adv data failed, error code = %x", ret);
            }
            adv_config_done |= ADV_CONFIG_FLAG;
            //config scan response data
            ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
            if (ret){
                ESP_LOGE(GATTS_TAG, "config scan response data failed, error code = %x", ret);
            }
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;
            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, DB_LEN, SVC_INST_ID);
            if (create_attr_ret){
                ESP_LOGE(GATTS_TAG, "create attr table failed, error code = %x", create_attr_ret);
            }
        }
       	    break;
        case ESP_GATTS_READ_EVT:
            ESP_LOGI(GATTS_TAG, "ESP_GATTS_READ_EVT");
       	    break;
        case ESP_GATTS_WRITE_EVT:
            ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, handle = %d, value len = %d, value :", param->write.handle, param->write.len);
            esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);
      	    break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            // the length of gattc prepare write data must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
            ESP_LOGI(GATTS_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
            //example_exec_write_event_env(&prepare_write_env, param);
            break;
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(GATTS_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            break;
        case ESP_GATTS_CONF_EVT:
            ESP_LOGI(GATTS_TAG, "ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
            break;
        case ESP_GATTS_START_EVT:
            ESP_LOGI(GATTS_TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
            // Lanzar el evento GATT_OK (COMM_CHANNEL_OK) para señalar que se ha alzaldo el servicio
            ESP_LOGD(GATTS_TAG, "%s: Alzando GATT_OK (COMM_CHANNEL_OK)", __func__);
            xEventGroupSetBits(nodo_evt_group_handle, COMM_CHANNEL_OK);
            break;
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
            esp_log_buffer_hex(GATTS_TAG, param->connect.remote_bda, 6);
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the iOS system, please refer to Apple official documents about the BLE connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
            conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
            conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
            //start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_SET_ATTR_VAL_EVT:
            ESP_LOGI(GATTS_TAG, "ESP_GATTS_SET_ATTR_VAL_EVT service:%x attr:%x status: %x", 
                    param->set_attr_val.srvc_handle,
                    param->set_attr_val.attr_handle,
                    param->set_attr_val.status);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(GATTS_TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != DB_LEN){
                ESP_LOGE(GATTS_TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, DB_LEN);
            }
            else {
                ESP_LOGI(GATTS_TAG, "create attribute table successfully, the number handle = %d",param->add_attr_tab.num_handle);
                memcpy(nodo_service_handles, param->add_attr_tab.handles, sizeof(nodo_service_handles));
                esp_ble_gatts_start_service(nodo_service_handles[ID_SVC]);
            }
            break;
        }
        case ESP_GATTS_STOP_EVT:
        case ESP_GATTS_OPEN_EVT:
        case ESP_GATTS_CANCEL_OPEN_EVT:
        case ESP_GATTS_CLOSE_EVT:
        case ESP_GATTS_LISTEN_EVT:
        case ESP_GATTS_CONGEST_EVT:
        case ESP_GATTS_UNREG_EVT:
        case ESP_GATTS_DELETE_EVT:
        default:
            break;
    }
}


static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            nodo_service_profile_tab[NODO_SERVICE_ID].gatts_if = gatts_if;
        } else {
            ESP_LOGE(GATTS_TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == nodo_service_profile_tab[idx].gatts_if) {
                if (nodo_service_profile_tab[idx].gatts_cb) {
                    nodo_service_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}
/*
 * Función para iniciar el servidor GATT de BLE
 */
void init_gatt_server(const EventGroupHandle_t evt_group, gatts_instance_id_t *instance_id)
{
    esp_err_t ret;
    /* Copiar el handle al groupo de eventos */
    nodo_evt_group_handle = evt_group;
    /* Copiar instance_id a la tabla, en caso de pasarlo como argumento */
    if ( instance_id != NULL ) {
        memcpy(instance_id_value, instance_id->instance_id, INSTANCE_ID_VAL_LEN);
        gatt_db[ID_INSTANCE_ID_VAL].att_desc.value = instance_id_value;
        ESP_LOGE(GATTS_TAG, "%s| Nuevo instance_id!", __func__);
        ESP_LOG_BUFFER_HEX(GATTS_TAG, instance_id->instance_id, instance_id->len);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(GATTS_TAG, "gatts register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(GATTS_TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(GATTS_APP_ID);
    if (ret){
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(MTU_SIZE);
    if (local_mtu_ret){
        ESP_LOGE(GATTS_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

}

void gatt_task(void *pvParameters) {
    /* TODO: Revisar */
    gatt_task_arg_t *arg = (gatt_task_arg_t *) pvParameters;
    xEventGroupWaitBits(arg->nodo_evt_group, COMM_CHANNEL_OK, pdFALSE, pdTRUE, portMAX_DELAY);
    xEventGroupSetBits(arg->nodo_evt_group, COMM_DISPATCHER_OK);
    ws_queue_msg_t msg = {0};  
    for(;;) {
        ESP_LOGD(GATT_TASK_TAG, "Esperando mensaje...");
        xQueueReceive(arg->out_queue, (void *) &msg, portMAX_DELAY);
        if (msg.type == MSG_MEAS_VECTOR_NORM) {
            for(uint8_t i = 0; i < msg.meas_vector->measure->len; i++) {
                measure_t measure = msg.meas_vector->measure->data[i];
                switch (measure.type) {
                    case TEMPERATURE:
                        esp_ble_gatts_set_attr_value(nodo_service_handles[ID_CHAR_TEMP_VAL], sizeof(int32_t), (uint8_t *) &measure.value);
                        break;
                    case HUMIDITY:
                        esp_ble_gatts_set_attr_value(nodo_service_handles[ID_CHAR_HUM_VAL], sizeof(int32_t), (uint8_t *) &measure.value);
                        break;
                    case LIGHT:
                        esp_ble_gatts_set_attr_value(nodo_service_handles[ID_CHAR_LUM_VAL], sizeof(int32_t), (uint8_t *) &measure.value);
                        break;
                    case PH:
                        esp_ble_gatts_set_attr_value(nodo_service_handles[ID_CHAR_PH_VAL], sizeof(int32_t), (uint8_t *) &measure.value);
                        break;
                    default:
                        ESP_LOGE(GATT_TASK_TAG, "Error: measure type incorrecto");
                }
            }
        }
    }
    vTaskDelete(NULL);
}
