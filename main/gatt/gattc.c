#include "gatt/gattc.h"

/** Funciones locales **/
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static uint16_t u16_from_bytes(const uint8_t *bytes, uint8_t len);
static uint32_t u32_from_bytes(const uint8_t *bytes, uint8_t len);
static void gattc_init_fail(void);

/* GATT Server char UUIDs */
static esp_bt_uuid_t remote_filter_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 =  REMOTE_SERVICE_UUID,},
};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

static uint8_t child_addr [MAC_ADDR_LEN];
static char child_addr_str [MAC_STR_LEN + 1];
static uint8_t instance_id[INSTANCE_ID_VAL_LEN];
static bool client_init_ok = false;
static bool connect    = false;
static bool get_server = false;
static gattc_discovery_cb_t discovery_cb = NULL;
// void *discovery_cb_arg;
/* Cola para enviar datos (ws) */
static QueueHandle_t out_queue;
/* Arreglo de estructuras con datos de las características */
static gattc_char_vec_t server_chars = { .chars = NULL, .len = 0 };
/* Variables para measure_vector */
static measure_t measure_data[3];
static measure_vector_t meas_vec = {.data = measure_data, .len = 3};
static ws_meas_vector_t ws_meas_vec = { .dev_addr = "GATTS_NODE_ADDR", .measure = &meas_vec };
/* Contenedor para mensajes al ws */
static ws_queue_msg_t ws_msg = { .type = MSG_MEAS_VECTOR_NORM, .meas_vector = &ws_meas_vec };
static SemaphoreHandle_t gattc_sem;

/* TODO: reducir ESP_LOGIs del módulo */


/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [NODO_PROFILE_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;
    static uint8_t read_chars = 0;
    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(GATTC_TAG, "REG_EVT");
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret){
            ESP_LOGE(GATTC_TAG, "set scan params error, error code = %x", scan_ret);
        }
        break;
    case ESP_GATTC_CONNECT_EVT:{
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
        /* Guardar conn_id en gl_profile_tab */
        gl_profile_tab[NODO_PROFILE_ID].conn_id = p_data->connect.conn_id;
        /* Copiar la direccion -- Bluetooth Device Address (BDA) -- al perfil (gl_profile_tab) */
        memcpy(gl_profile_tab[NODO_PROFILE_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTC_TAG, "REMOTE BDA:");
        esp_log_buffer_hex(GATTC_TAG, gl_profile_tab[NODO_PROFILE_ID].remote_bda, sizeof(esp_bd_addr_t));
        /* Solicitar Minimum Transfer Unit (MTU) */
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
        if (mtu_ret){
            ESP_LOGE(GATTC_TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    }
    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "open failed, status %d", p_data->open.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "open success");
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        ESP_LOGI(GATTC_TAG, "%s: ESP_GATTC_DIS_SRVC_CMPL_EVT", __func__);
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "discover service failed, status %d", param->dis_srvc_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "discover service complete conn_id %d", param->dis_srvc_cmpl.conn_id);
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &remote_filter_service_uuid);
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG,"config mtu failed, error status = %x", param->cfg_mtu.status);
        }
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        ESP_LOGI(GATTC_TAG, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGI(GATTC_TAG, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_UUID) {
            ESP_LOGI(GATTC_TAG, "service found");
            get_server = true;
            /* Obtener start/end handles de la tabla de servicios del servidor */
            gl_profile_tab[NODO_PROFILE_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[NODO_PROFILE_ID].service_end_handle = p_data->search_res.end_handle;
            ESP_LOGI(GATTC_TAG, "UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        ESP_LOGI(GATTC_TAG, "%s: ESP_GATTC_SEARCH_CMPL_EVT", __func__);
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        /* > Mostrar de donde se obtuvieron los datos del prefil */
        if(p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
            ESP_LOGI(GATTC_TAG, "Get service information from remote device");
        } else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) {
            ESP_LOGI(GATTC_TAG, "Get service information from flash");
        } else {
            ESP_LOGI(GATTC_TAG, "unknown service source");
        }
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        if (get_server){
            esp_gattc_char_elem_t char_elem_result = {0};
            uint16_t count = 1;
            for( uint8_t i = 0; i < server_chars.len; i++) {
                //char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if ( server_chars.chars[i].handle == 0 ) {
                    ESP_LOGI(GATTC_TAG, "%s| Looking for char by UUID: 0x%02x", __func__, server_chars.chars[i].uuid.uuid.uuid16);
                    esp_gatt_status_t status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gl_profile_tab[NODO_PROFILE_ID].service_start_handle,
                                                             gl_profile_tab[NODO_PROFILE_ID].service_end_handle,
                                                             server_chars.chars[i].uuid,
                                                             &char_elem_result,
                                                             &count);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_char_by_uuid error");
                        esp_ble_gattc_close(gattc_if, p_data->search_cmpl.conn_id);
                        return;
                    } else {
                        server_chars.chars[i].handle = char_elem_result.char_handle;
                    }
                }
                esp_err_t ret = esp_ble_gattc_read_char(gattc_if,
                        p_data->search_cmpl.conn_id,
                        server_chars.chars[i].handle,
                        ESP_GATT_AUTH_REQ_NONE);
                if(ret != ESP_OK) {
                    ESP_LOGE(GATTC_TAG, "Error leyendo temperatura %s", esp_err_to_name(ret));
                }
            }
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT");
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        esp_log_buffer_hex(GATTC_TAG, bda, sizeof(esp_bd_addr_t));
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "write char failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "write char success ");
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        connect = false;
        get_server = false;
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);
        break;
    case ESP_GATTC_READ_CHAR_EVT:
        ESP_LOGI(GATTC_TAG, "READ_CHAR_EVT");
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "read char failed, error status = %s", esp_err_to_name(p_data->write.status));
            break;
        }
        esp_log_buffer_hex(GATTC_TAG, p_data->read.value, p_data->read.value_len);
        for(uint8_t i = 0; i < server_chars.len; i++) {
            if ( server_chars.chars[i].handle == p_data->read.handle && p_data->read.value != NULL ) {
                if ( server_chars.chars[i].uuid.uuid.uuid16 == TEST_DISCOVERY_UUID ) {
                    uint8_t *char_buffer = calloc(p_data->read.value_len, sizeof(uint8_t));
                    if ( char_buffer == NULL ) {
                        ESP_LOGE(GATTC_TAG, "%s| Error alojando espacio para característica (len > 2)!", __func__);
                    }
                    memcpy(char_buffer, p_data->read.value, p_data->read.value_len);
                    server_chars.chars[i].value_str = char_buffer;
                } else {
                    if ( p_data->read.value_len == 2 ) {
                    uint16_t temp = u16_from_bytes(p_data->read.value, p_data->read.value_len);
                    ESP_LOGI(GATTC_TAG, "%s| u16_from_bytes: %u", __func__, temp);
                    server_chars.chars[i].value_u16 = temp;
                    } else if ( p_data->read.value_len == 4 ) {
                        uint32_t temp = u32_from_bytes(p_data->read.value, p_data->read.value_len);
                        ESP_LOGI(GATTC_TAG, "%s| u32_from_bytes: %u", __func__, temp);
                        server_chars.chars[i].value_u32 = temp;
                    }
                }
                read_chars += 1;
                ESP_LOGI(GATTC_TAG, "Read ok, handle = 0x%02x", p_data->read.handle);
            }
        }
        /* Verificar si ya se leyeron todas las propiedades */
        if ( read_chars == server_chars.len && read_chars != 0 ) {
            gattc_submit_chars();
            read_chars = 0;
        }
        ESP_LOGI(GATTC_TAG, "%s| ESP_GATTC_READ_CHAR_EVT finished...", __func__);
        break;
    default:
        break;
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        client_init_ok = true;
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTC_TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "scan start success");

        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            //esp_log_buffer_hex(GATTC_TAG, scan_result->scan_rst.bda, 6);
            ESP_LOGI(GATTC_TAG, "searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
            /* Ver /components/bt/host/bluedroid/api/include/api/esp_gap_ble_api.h:228 para resolver otros tipos */
            /* adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len); */
            //ESP_LOGI(GATTC_TAG, "searched Device Name Len %d", adv_name_len);
            //esp_log_buffer_char(GATTC_TAG, adv_name, adv_name_len);
            /* ¿La dirección del disp. detectado es la del que buscamos? */
            if (memcmp( scan_result->scan_rst.bda, child_addr, 6) == 0) {
                ESP_LOGI(GATTC_TAG, "Dispositivo encontrado!");
                if (connect == false) {
                    connect = true;
                    esp_ble_gap_stop_scanning();
                    esp_ble_gattc_open(gl_profile_tab[NODO_PROFILE_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                }
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            ESP_LOGI(GATTC_TAG, "Scan done!");
            if ( connect == false ) {
                ESP_LOGE(GATTC_TAG, "Not found!");
                gattc_init_fail();
                xSemaphoreGive(gattc_sem);
            }
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GATTC_TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "stop scan successfully");
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GATTC_TAG, "adv stop failed, error status = %x", param->adv_stop_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "stop adv successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(GATTC_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
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

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(GATTC_TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == gl_profile_tab[idx].gattc_if) {
                if (gl_profile_tab[idx].gattc_cb) {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

int init_gatt_client(const QueueHandle_t queue) {
    out_queue = queue;
    //register the  callback function to the gap module
    esp_err_t ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret){
        ESP_LOGE(GATTC_TAG, "%s gap register failed, error code = %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }
    //register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if(ret){
        ESP_LOGE(GATTC_TAG, "%s gattc register failed, error code = %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }

    ret = esp_ble_gattc_app_register(NODO_PROFILE_ID);
    if (ret){
        ESP_LOGE(GATTC_TAG, "%s gattc app register failed, error code = %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret){
        ESP_LOGE(GATTC_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }
    /* Crear semáforo para controlar lecturas */
    gattc_sem = xSemaphoreCreateBinary(); 
    if ( gattc_sem == NULL ) {
        ESP_LOGE(GATTC_TAG, "%s| Semaforo no creado!", __func__);
        return -1;
    }
    xSemaphoreGive(gattc_sem);
    return 0;
}

void gattc_set_instance_id(const uint8_t *inst_id) {
    memcpy(instance_id, inst_id, INSTANCE_ID_VAL_LEN);
    ESP_LOGW(GATTC_TAG, "%s| instance_id:", __func__);
    ESP_LOG_BUFFER_HEX(GATTC_TAG, instance_id, INSTANCE_ID_VAL_LEN);
}

void gattc_set_addr(const uint8_t *raw_addr, const char *str_addr) {
    /* Guardar la dirección del nodo hijo (raw) */
    memcpy(child_addr, raw_addr, MAC_ADDR_LEN);
    ESP_LOGI(GATTC_TAG, "%s| Declarando dirección (raw)...", __func__);
    ESP_LOG_BUFFER_HEX(GATTC_TAG, child_addr, MAC_ADDR_LEN);
    /* Guardar la dirección del nodo hijo (cadena) */
    memcpy(child_addr_str, str_addr, MAC_STR_LEN);
    child_addr_str[MAC_STR_LEN] = '\0';
    ESP_LOGI(GATTC_TAG, "%s| Declarando dirección (strings)...", __func__);
}

/* Llamar antes de nodo_ble_start */
int gattc_set_chars(const uint16_t *chars, uint8_t chars_len) {
    ESP_LOGI(GATTC_TAG, "%s| Declarando caraterísticas...", __func__);
    /* Crear arreglo de estructuras */
    if (server_chars.chars == 0 || server_chars.chars == NULL) {
        ESP_LOGI(GATTC_TAG, "%s| Alojando client_chars.chars...", __func__);
        gattc_char_t *tmp = calloc(chars_len, sizeof(gattc_char_t)); 
        if ( tmp == NULL ) {
            ESP_LOGE(GATTC_TAG, "%s| Error alojando client_chars.chars!", __func__);
            return -1;
        }
        server_chars.chars = tmp;
    } else {
        if ( ( server_chars.chars = realloc(server_chars.chars, chars_len ) ) == NULL ) {
            ESP_LOGE(GATTC_TAG, "%s| Error realojando client_chars.chars!", __func__);
            return -1;
        }
    }
    ESP_LOGI(GATTC_TAG, "%s| Asignando client_chars.chars...", __func__);
    for(uint8_t i = 0; i < chars_len; i++) {
        server_chars.chars[i].uuid.len = ESP_UUID_LEN_16;
        server_chars.chars[i].uuid.uuid.uuid16 = chars[i];
    }
    server_chars.len = chars_len;
    return 0;
}

/* Llamar una vez que se ha llamado a la función set_params */
int nodo_gattc_start(void) {
    if ( !client_init_ok ) {
        ESP_LOGE(GATTC_TAG, "%s| Cliente no inicializado!", __func__);
        return -1;
    }
    xSemaphoreTake(gattc_sem, portMAX_DELAY);
    vTaskDelay(500);
    ESP_LOGI(GATTC_TAG, "%s| Start scanning!", __func__);
    esp_ble_gap_start_scanning(GATTC_SCAN_TIMEOUT);
    /* Tomar el semáforo solo para esperar */
    xSemaphoreTake(gattc_sem, portMAX_DELAY);
    xSemaphoreGive(gattc_sem);
    return 0;
}

/* Convertir los primeros dos bytes de un arreglo entero s/signo de 16b */
static uint16_t u16_from_bytes(const uint8_t *bytes, uint8_t len) {
    return len <= 2 ? ( ( ( 0L | bytes[1] ) << 8 ) | bytes[0] ) : 0;
}

/* Convertir los primeros dos bytes de un arreglo entero s/signo de 32b */
static uint32_t u32_from_bytes(const uint8_t *bytes, uint8_t len) {
    if ( len <= 4 ) {
        uint32_t aux, base = 0L;
        for(uint8_t i = 0; i < len; i++) {
            aux = 0L | ( bytes[i] << 8 * i );
            base |= aux;
        }
        return base;
    } else {
        return 0;
    }
}

void gattc_init_fail(void) {
    ws_queue_msg_t msg = { .type = MSG_STATUS_GENERIC, .status = { .esp_status = ESP_FAIL, .type = DEV_DISCOVERY_CMPL }};
    xQueueSendToBack(out_queue, &msg, portMAX_DELAY);
}

int gattc_submit_chars(void) {
    ESP_LOGW(GATTC_TAG, "%s| Submit chars!", __func__); 
    if ( server_chars.len == 1 
            && server_chars.chars[0].uuid.uuid.uuid16 == TEST_DISCOVERY_UUID ) {
        bool discovery_ok = false;
        ws_queue_msg_t disco_msg = { .type = MSG_STATUS_GENERIC, 
            .status = { .type = DEV_DISCOVERY_CMPL, .esp_status = ESP_FAIL }};
        ESP_LOGW(GATTC_TAG, "%s| DISCOVERY_CMPL", __func__); 
        /* Verificar instance ID */
        ESP_LOG_BUFFER_HEX(GATTC_TAG, server_chars.chars[0].value_str, INSTANCE_ID_VAL_LEN);
        ESP_LOG_BUFFER_HEX(GATTC_TAG, instance_id, INSTANCE_ID_VAL_LEN);
        if ( memcmp(server_chars.chars[0].value_str, instance_id, INSTANCE_ID_VAL_LEN) != 0 ) {
            ESP_LOGE(GATTC_TAG, "%s| Error verificando Instace ID!", __func__);
        } else {
            ESP_LOGW(GATTC_TAG, "%s| Nuevo dispositivo verificado!", __func__);
            ESP_LOG_BUFFER_HEX(GATTC_TAG, instance_id, INSTANCE_ID_VAL_LEN);
            /* Enviar mensaje DISCOVERY_CMPL */
            disco_msg.status.esp_status = ESP_OK;
            discovery_ok = true;
        }
        /* Evaluar antes del reinicio (discovery_cb) */
        xQueueSendToBack(out_queue, &disco_msg, portMAX_DELAY);
        esp_ble_gattc_close(gl_profile_tab[NODO_PROFILE_ID].gattc_if, gl_profile_tab[NODO_PROFILE_ID].conn_id);
        if ( discovery_ok == false ) { /* Liberar solamente si no se pudo descubrir -> no reinicio */
            memset(child_addr, 0, sizeof(child_addr)/sizeof(child_addr[0]));
            memset(child_addr_str, 0, sizeof(child_addr_str)/sizeof(child_addr_str[0]));
            memset(instance_id, 0, INSTANCE_ID_VAL_LEN);
        }
        free(server_chars.chars[0].value_str);
        xSemaphoreGive(gattc_sem);
        if ( discovery_cb != NULL ) { 
            discovery_cb( ( discovery_ok == true) ? DISCOVERY_CMPL : DISCOVERY_CMPL_FAIL, child_addr); /* Llamar al final por reinicio! */
        }
    } else {
        ESP_LOGW(GATTC_TAG, "%s| MEAS_MSG", __func__); 
        if ( server_chars.len > meas_vec.len ) {
            ESP_LOGE(GATTC_TAG, "%s| Error - chars > vector de mediciones!", __func__);
            return -1;
        }
        for(uint8_t i = 0; i < server_chars.len ; i++) {
            switch( server_chars.chars[i].uuid.uuid.uuid16 ) {
                case TEMP_CHAR_UUID:
                    ESP_LOGI(GATTC_TAG, "%s : %u", "TEMPERATURE", server_chars.chars[i].value_u16);
                    measure_data[i].type = TEMPERATURE;
                    measure_data[i].value_u32 = server_chars.chars[i].value_u32;
                    break;
                case HUMIDITY_CHAR_UUID:
                    measure_data[i].type = HUMIDITY;
                    ESP_LOGI(GATTC_TAG, "%s : %u", "HUMIDITY", server_chars.chars[i].value_u16);
                    measure_data[i].value_u32 = server_chars.chars[i].value_u32;
                    break;
                case LUX_CHAR_UUID:
                    measure_data[i].type = LIGHT;
                    ESP_LOGI(GATTC_TAG, "%s : %u", "LIGHT", server_chars.chars[i].value_u16);
                    measure_data[i].value_u16 = server_chars.chars[i].value_u16;
                    break;
                case PH_CHAR_UUID:
                    measure_data[i].type = PH;
                    ESP_LOGI(GATTC_TAG, "%s : %u", "PH", server_chars.chars[i].value_u16);
                    measure_data[i].value_u32 = server_chars.chars[i].value_u32;
                    break;
                default:
                    ESP_LOGE(GATTC_TAG, "%s| UUID no encontrada!", __func__);
                    continue;
            }
        }
        ws_meas_vec.dev_addr = child_addr_str;
        ESP_LOGI(GATTC_TAG, "Enviando datos...");
        xQueueSendToBack(out_queue, &ws_msg, portMAX_DELAY);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_ble_gattc_close(gl_profile_tab[NODO_PROFILE_ID].gattc_if, gl_profile_tab[NODO_PROFILE_ID].conn_id);
    xSemaphoreGive(gattc_sem);
    return 0;
}

void  register_discovery_cb(gattc_discovery_cb_t disco_cb) {
    discovery_cb = disco_cb;
}

void unregister_discovery_cb(void) {
    discovery_cb = NULL;
}
