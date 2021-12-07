#ifndef STORAGE_NVS_H
#define STORAGE_NVS_H

#include <stdlib.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp32/nodo_def.h"
#include "nvs.h"

#define NVS_TAG             "NODO-NVS"
#define AP_NAMESPACE        "AP_NS"             // Particion en la NVS para guardar info. sobre el punto de acceso WiFi
#define AP_SSID             "AP_SSID"           // Entrada en la memoria NVS para el SSID
#define AP_PSK              "AP_PSK"            // Entrada en la memoria NVS para la PSK
#define TOKEN_NAMESPACE     "TOKEN_NS"          // Particion en la NVS para guardar info. sobre el Token para acceder a los WS
#define TOKEN               "TOKEN"             // Entrada en la memoria NVS para el token
#define DEV_CHAR_NAMESPACE  "DEV_NS"            // Namespace para guardar el tipo de dispositivo
#define DEV_TYPE            "DEV_TYPE"          // Tipo de dispositivo

/** Funciones **/
int nvs_save_wifi_credentials(char *ssid, char *psk);
int nvs_save_token(char *token);
int nvs_get_wifi_credentials(uint8_t **ssid, uint8_t **psk);
uint8_t *nvs_get_token();
int nvs_set_mode(enum dev_type_t mode);
uint8_t nvs_get_mode(void);
#endif
