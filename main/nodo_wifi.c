#include "nodo_wifi.h"
/**
 * TODO: 
 *      + Documentar c:
 *      + Limpiar buffer para enviar contraseñas WiFi
 * Inicialización hasta la fase 3. Start
 */

int conn_retry_num = 0;
static int ap_cmp(const void *ap1,const void *ap2);

/*
 * Secuencia de inicialización del controlador WiFI
 * Ver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#esp32-wi-fi-station-general-scenario
 */
void nodo_wifi_init(nodo_wifi_conn_cb_t on_conn_cb, EventGroupHandle_t evt_group) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // Ensamblar el argumento para la función callback 'on_conn_cb'
    wifi_event_handler_arg_t *arg = (wifi_event_handler_arg_t *) calloc(1, sizeof(wifi_event_handler_arg_t));
    arg->event_group = evt_group;
    arg->on_conn_cb = on_conn_cb;
    //wifi_event_handler_arg_t arg = { .event_group = evt_group, .on_conn_cb = &on_conn_cb };
    /*
     * Registrar event handler para eventos del controlador.
     * Ver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#esp32-wi-fi-event-description
     * Todos los eventos WiFi
     */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                ESP_EVENT_ANY_ID,
                &wifi_event_handler,
                //(void *) on_conn_cb, [!!!] Pasar el callback solo como nodo_wifi_cb_t porque ya es un ptr
                (void *) arg,
                NULL));
    // Evento: Obtener una IP
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                IP_EVENT_STA_GOT_IP,
                &wifi_event_handler,
                //(void *) on_conn_cb,
                (void *) arg,
                NULL));
    /*
     * Establecer modo de conexión (estación)
     */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

nodo_ap_scan_results_t nodo_wifi_scan() {
    nodo_ap_scan_results_t res = {0};
    uint16_t ap_count = 0;
    uint16_t ap_info_len = MAX_AP_SCAN_SIZE;
    wifi_ap_record_t *ap_info = calloc(MAX_AP_SCAN_SIZE, sizeof(wifi_ap_record_t));
    for (uint8_t i = 0; i < MAX_SCAN_ATTEMPTS; i++) {
        ESP_LOGI(WIFI_TAG, "Buscando APs intento #%d", i);
        // Iniciar la funcion de búsqueda de puntos de acceso (APs)
        esp_wifi_scan_start(NULL, true);
        // Obtener los datos de los APs encontrados después
        // de realizar la búsqueda
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_info_len, ap_info));
        // Asignar el número de APs encontradas a la variable 
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        esp_wifi_scan_stop();
        if ( ap_count > 0 ) {
            ESP_LOGI(WIFI_TAG, "Total APs scanned = %u", ap_count);
            res.len = ap_count;
            res.results = ap_info;
            for (size_t i = 0; (i < MAX_AP_SCAN_SIZE) && (i < ap_count); i++) {
                ESP_LOGI(WIFI_TAG, "SSID \t\t%s", ap_info[i].ssid);
                //ESP_LOGI(WIFI_TAG, "RSSI \t\t%d", ap_info[i].rssi);
                //print_auth_mode(ap_info[i].authmode);
                //if (ap_info[i].authmode != WIFI_AUTH_WEP) {
                //    print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
                //}
                //ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
            }
            /** Ordenar **/
            qsort(res.results, ap_count, sizeof(wifi_ap_record_t), ap_cmp);
            break;
        }
    }
    return res;
}

/* Función auxiliar para comparar entradas de APs, utilizada para ordenar */
static int ap_cmp(const void *ap1, const void *ap2) {
    wifi_ap_record_t *ap1_aux = (wifi_ap_record_t *) ap1;
    wifi_ap_record_t *ap2_aux = (wifi_ap_record_t *) ap2;
    return ap1_aux->rssi > ap2_aux->rssi;
}

void nodo_wifi_set_credentials(uint8_t *ssid, uint8_t *psk) {
    ESP_EARLY_LOGI(WIFI_TAG, "%s| Intentando conexión con credenciales %s:%s", __func__, ssid, psk);
    // Estructura de configuración para AP
    wifi_config_t wifi_config = {
        .sta = {
            .ssid =  {0} ,
            .password = {0},
            // Mínimo nivel de seguridad para conectarse a un AP
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
            .listen_interval = 4, // Aumentar el intervalo para recibir beacons
        },
    };
    memcpy(wifi_config.sta.ssid, ssid, strlen((char*) ssid));
    memcpy(wifi_config.sta.password, psk, strlen((char*) psk));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    esp_err_t ret = esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    if ( ret != ESP_OK ) {
        ESP_EARLY_LOGE(WIFI_TAG, "%s| Error configurando power saving!", __func__);
    }
    ESP_EARLY_LOGI(WIFI_TAG, "Conectando a %s...", ssid);
    esp_wifi_connect();
}

/*
 * Función para manejar los eventos generados por el controlador WiFi
 */
void wifi_event_handler(void* arg, esp_event_base_t event_base,
        int32_t event_id, void* event_data)
{
    // Obtener el argumento de la función
    wifi_event_handler_arg_t *cb_arg = (wifi_event_handler_arg_t *) arg;
    //nodo_wifi_conn_cb_t on_conn_cb = (nodo_wifi_conn_cb_t) arg;
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        conn_retry_num = 0;
        // Notificar que se tiene una conexión
        // (*on_conn_cb)(NULL, CONN_OK); // <== Invocar la función que apunta el ptr on_conn_cb pasado como argumento
        (*cb_arg->on_conn_cb)(cb_arg->event_group, CONN_OK);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (conn_retry_num < MAX_CONNECT_ATTEMPTS) {
            esp_wifi_connect();
            conn_retry_num++;
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
        } else {
            ESP_LOGI(WIFI_TAG,"connect to the AP fail");
            //(*on_conn_cb)(NULL, CONN_FAIL);
            (*cb_arg->on_conn_cb)(cb_arg->event_group, CONN_FAIL);
        }
    }
}

uint8_t get_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(WIFI_TAG, "Authmode \tWIFI_AUTH_OPEN");
        return NODO_WIFI_AUTH_OPEN;
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(WIFI_TAG, "Authmode \tWIFI_AUTH_WEP");
        return NODO_WIFI_AUTH_WEP;
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(WIFI_TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        return NODO_WIFI_AUTH_WPA_PSK;
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(WIFI_TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        return NODO_WIFI_AUTH_WPA2_PSK;
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(WIFI_TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        return NODO_WIFI_AUTH_WPA_WPA2_PSK;
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(WIFI_TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        return NODO_WIFI_AUTH_WPA3_PSK;
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(WIFI_TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        return NODO_WIFI_AUTH_WPA2_WPA3_PSK;
        break;
    default:
        ESP_LOGI(WIFI_TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        return NODO_AUTH_UNKNOWN;
        break;
    }
}

int nodo_wifi_disable(void) {
    esp_err_t ret = esp_wifi_disconnect();
    if ( ret == ESP_ERR_WIFI_NOT_INIT ) {
        return 0;
    }
    if ( ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED ) {
        ESP_LOGE(WIFI_TAG, "%s: Error desconectando [code %s]", __func__, esp_err_to_name(ret));
    }
    ret = esp_wifi_stop();
    if ( ret != ESP_OK ) {
        ESP_LOGE(WIFI_TAG, "%s: Error parando controlador [code %s]", __func__,esp_err_to_name(ret));
        return -1;
    }
    ret = esp_wifi_deinit();
    if ( ret != ESP_OK ) {
        ESP_LOGE(WIFI_TAG, "%s: Error deshabilitando controlador [code %s]", __func__,esp_err_to_name(ret));
        return -1;
    }
    return 0;
}
