#ifndef NODO_NVS_H
#define NODO_NVS_H

#include <stdlib.h>
#include "nvs.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nodo_def.h"

#define NVS_TAG             "NODO-NVS"
#define NVS_AP_PART         "NVS_AP_PART"       // Particion en la NVS para guardar info. sobre el punto de acceso WiFi
#define AP_SSID             "AP_SSID"           // Entrada en la memoria NVS para el SSID
#define AP_PSK              "AP_PSK"            // Entrada en la memoria NVS para la PSK
#define NVS_TOKEN_PART      "NVS_TOKEN_PART"    // Particion en la NVS para guardar info. sobre el Token para acceder a los WS
#define TOKEN               "TOKEN"             // Entrada en la memoria NVS para el token
#define NVS_DEV_TYPE_NS     "NVS_TOKEN_PART"    // Namespace para guardar el tipo de dispositivo
#define DEV_TYPE            "DEV_TYPE"          // Tipo de dispositivo

/** Funciones **/
int nvs_save_wifi_credentials(char *ssid, char *psk);
int nvs_save_token(char *token);
int nvs_get_wifi_credentials(uint8_t **ssid, uint8_t **psk);
uint8_t *nvs_get_token();
int nvs_set_mode(enum dev_type_t mode);
uint8_t nvs_get_mode(void);
#endif
