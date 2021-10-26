#include "nodo_nvs.h"

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
    esp_err_t ret = nvs_open(NVS_AP_PART, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error abriendo partición: %s", NVS_AP_PART);
        return -1;
    }
    ret = nvs_set_str(nvs_handle, AP_SSID, ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error guardando %s [%s]",AP_SSID, ssid);
        return -1;
    }
    ret = nvs_set_str(nvs_handle, AP_PSK, psk);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error guardando %s [%s]", AP_PSK, psk);
        return -1;
    }
    // Esperar a que se llame nvs_save_token() para guardar las credenciales
    //ret = nvs_commit(nvs_handle);
    //if (ret != ESP_OK) {
    //    ESP_LOGE(NVS_TAG, "Error ejecutando registros");
    //    return -1;
    //}
    nvs_close(nvs_handle);
    return 0;
}

int nvs_get_wifi_credentials(uint8_t **ssid, uint8_t **psk) {
    nvs_handle_t nvs_handle;
    size_t str_len;
    esp_err_t ret = nvs_open(NVS_AP_PART, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error abriendo partición NVS_AP_PART");
        return -1;
    }
    // SSID
    ret = nvs_get_str(nvs_handle, AP_SSID, NULL, &str_len);
    if (ret != ESP_OK) { return -1; }
    *ssid = calloc(str_len, sizeof(uint8_t));
    ret = nvs_get_str(nvs_handle, AP_SSID, (char *) *ssid, &str_len);
    if (ret != ESP_OK) { 
        free(*ssid);
        return -1; 
    }
    // PSK
    ret = nvs_get_str(nvs_handle, AP_PSK, NULL, &str_len);
    if (ret != ESP_OK) { return -1; }
    ssid = calloc(str_len, sizeof(uint8_t));
    ret = nvs_get_str(nvs_handle, AP_PSK, (char *) *psk, &str_len);
    if (ret != ESP_OK) { 
        free(*ssid);
        free(*psk);
        return -1; 
    }
    return 0;
    nvs_close(nvs_handle);
}

int nvs_save_token(char *token) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_TOKEN_PART, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error abriendo partición: %s", NVS_TOKEN_PART);
        return -1;
    }
    ret = nvs_set_str(nvs_handle, TOKEN, token);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error guardando %s [%s]", TOKEN, token);
        return -1;
    }
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error ejecutando registros");
        return -1;
    }
    nvs_close(nvs_handle);
    return 0;
}

uint8_t *nvs_get_token() {
    nvs_handle_t nvs_handle;
    uint8_t *token;
    size_t str_len;
    esp_err_t ret = nvs_open(NVS_TOKEN_PART, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(NVS_TAG, "nvs_get_token: Error abriendo partición %s", NVS_TOKEN_PART);
        return NULL;
    }
    // Leer token
    ret = nvs_get_str(nvs_handle, TOKEN, NULL, &str_len);
    if (ret != ESP_OK) { 
        ESP_LOGE(NVS_TAG, "nvs_get_token: Error leyendo longitud de token");
        return NULL; 
    }
    token = (uint8_t *) calloc(str_len, sizeof(uint8_t));
    ret = nvs_get_str(nvs_handle, TOKEN, (char *) token, &str_len);
    if (ret != ESP_OK) { 
        ESP_LOGE(NVS_TAG, "nvs_get_token: Error leyendo token");
        free(token);
        return NULL; 
    }
    nvs_close(nvs_handle);
    return token;
}
