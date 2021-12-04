#include "storage/nodo_nvs.h"

/*
 * Guardar las credenciales (SSID/PSK) para la conexión WiFi del dispositivo
 * en la memoria no-volatil (NVS).
 *
 * Nota: Estos datos son guardados de forma plana (sin ninguna encripción)
 *
 * Argumentos:
 *      ssid: Cadena que representa el SSID del punto de acceso (AP)
 *      psk: Cadena que representa la contraseña (clave pre-compartida) 
 *      para el AP
 */
int nvs_save_wifi_credentials(char *ssid, char *psk) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(AP_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error abriendo %s [code %s]", __func__, AP_NAMESPACE, esp_err_to_name(ret));
        return -1;
    }
    ret = nvs_set_str(nvs_handle, AP_SSID, ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error guardando %s [code %s]",__func__, AP_SSID, esp_err_to_name(ret));
        return -1;
    }
    ret = nvs_set_str(nvs_handle, AP_PSK, psk);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error guardando %s [code %s]", __func__, AP_PSK, esp_err_to_name(ret));
        return -1;
    }
    // Esperar a que se llame nvs_save_token() para guardar las credenciales
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error ejecutando registros");
        return -1;
    }
    nvs_close(nvs_handle);
    return 0;
}

/*
 * Obtener las credenciales WiFi, almacenadas en el almacenamiento NVS
 * 
 * Argumentos:
 *      ssid : Puntero doble indicando una región de memoria para almacenar el
 *      valor del SSID
 *      psk : Puntero doble indicando una región de memoria para almacenar el
 *      valor de la PSK
 */
int nvs_get_wifi_credentials(uint8_t **ssid, uint8_t **psk) {
    nvs_handle_t nvs_handle;
    size_t str_len;
    esp_err_t ret = nvs_open(AP_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error abriendo %s [code %s]", __func__, AP_NAMESPACE, esp_err_to_name(ret));
        return -1;
    }
    // SSID
    ret = nvs_get_str(nvs_handle, AP_SSID, NULL, &str_len);
    if (ret != ESP_OK) { return -1; }
    *ssid = (uint8_t *) calloc(str_len, sizeof(uint8_t));
    ret = nvs_get_str(nvs_handle, AP_SSID, (char *) *ssid, &str_len);
    if (ret != ESP_OK) { 
        ESP_LOGE(NVS_TAG, "%s: Error abriendo %s [code %s]", __func__, AP_SSID, esp_err_to_name(ret));
        free(*ssid);
        return -1; 
    }
    // PSK
    ret = nvs_get_str(nvs_handle, AP_PSK, NULL, &str_len);
    if (ret != ESP_OK) { return -1; }
    *psk = (uint8_t *) calloc(str_len, sizeof(uint8_t));
    ret = nvs_get_str(nvs_handle, AP_PSK, (char *) *psk, &str_len);
    if (ret != ESP_OK) { 
        ESP_LOGE(NVS_TAG, "%s: Error abriendo %s [code %s]", __func__, AP_PSK, esp_err_to_name(ret));
        free(*ssid);
        free(*psk);
        return -1; 
    }
    return 0;
    nvs_close(nvs_handle);
}

/*
 *  Guardar el token para servicios web en la memoria NVS
 *
 *  Argumentos:
 *      token: Puntero al arreglo de carácteres que representan al token
 *  Retorno:
 *      En ejecución exitosa 0, de lo contrario -1.
 */
int nvs_save_token(char *token) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(TOKEN_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error abriendo partición: %s [code %s]", __func__, TOKEN_NAMESPACE, esp_err_to_name(ret));
        return -1;
    }
    ret = nvs_set_str(nvs_handle, TOKEN, token);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error guardando %s [code %s]", __func__,TOKEN, esp_err_to_name(ret));
        return -1;
    }
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error ejecutando registros [code %s]", __func__, esp_err_to_name(ret));
        return -1;
    }
    nvs_close(nvs_handle);
    return 0;
}

/*
 *  Obtener el valor del token, almacenado en la memoria NVS
 *
 *  Retorno:
 *      Puntero a un arreglo de bytes (uint8_t) a una región de memoria
 *      dinámica donde se encuentra el token. NULL en caso de algún error
 */
uint8_t *nvs_get_token() {
    nvs_handle_t nvs_handle;
    uint8_t *token;
    size_t str_len;
    esp_err_t ret = nvs_open(TOKEN_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error abriendo partición %s [code %s]", __func__, TOKEN_NAMESPACE, esp_err_to_name(ret));
        return NULL;
    }
    // Leer token
    ret = nvs_get_str(nvs_handle, TOKEN, NULL, &str_len);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error leyendo longitud de token [code %s]", __func__, esp_err_to_name(ret));
        return NULL; 
    }
    token = (uint8_t *) calloc(str_len, sizeof(uint8_t));
    ret = nvs_get_str(nvs_handle, TOKEN, (char *) token, &str_len);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error leyendo token [code %s]", __func__, esp_err_to_name(ret));
        free(token);
        return NULL; 
    }
    nvs_close(nvs_handle);
    return token;
}

int nvs_set_mode(enum dev_type_t mode) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(DEV_CHAR_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error abriendo namespace: %s [code %s]", 
                __func__, DEV_CHAR_NAMESPACE, esp_err_to_name(ret));
        return -1;
    }
    ret = nvs_set_u8(nvs_handle, DEV_TYPE, mode);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error guardando %s [%s]", 
                __func__, DEV_TYPE, esp_err_to_name(ret));
        return -1;
    }
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error ejecutando registros [code %s]", __func__, esp_err_to_name(ret));
        return -1;
    }
    nvs_close(nvs_handle);
    return 0;
}

uint8_t nvs_get_mode(void) {
    nvs_handle_t nvs_handle;
    uint8_t dev_type;
    esp_err_t ret = nvs_open(DEV_CHAR_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "%s: Error abriendo namespace %s [code %s]" 
                ,__func__, DEV_CHAR_NAMESPACE , esp_err_to_name(ret));
        return 0;
    }
    ret = nvs_get_u8(nvs_handle, DEV_TYPE, &dev_type);
    if (ret != ESP_OK) { 
        ESP_LOGE(NVS_TAG, "%s: Error leyendo %s", __func__, DEV_TYPE);
        return 0; 
    }
    nvs_close(nvs_handle);
    return dev_type;
}
