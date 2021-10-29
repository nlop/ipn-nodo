#ifndef NODO_NVS_H
#define NODO_NVS_H

#include <stdlib.h>
#include "nvs.h"
#include "esp_err.h"
#include "esp_log.h"

#define NVS_TAG             "NODO-NVS"
#define AP_NAMESPACE        "AP_NAMESPACE"      // Particion en la NVS para guardar info. sobre el punto de acceso WiFi
#define AP_SSID             "AP_SSID"           // Entrada en la memoria NVS para el SSID
#define AP_PSK              "AP_PSK"            // Entrada en la memoria NVS para la PSK
#define TOKEN_NAMESPACE     "TOKEN_NAMESPACE"   // Particion en la NVS para guardar info. sobre el Token para acceder a los WS
#define TOKEN               "TOKEN"             // Entrada en la memoria NVS para el token

/** Funciones **/
int nvs_save_wifi_credentials(char *ssid, char *psk);
int nvs_save_token(char *token);
int nvs_get_wifi_credentials(uint8_t **ssid, uint8_t **psk);
uint8_t *nvs_get_token();
#endif
