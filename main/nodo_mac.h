#ifndef NODO_MAC_H
#define NODO_MAC_H

#define MAC_BYTES               6                       // Número de bytes que conforman la dirección MAC
#define MAC_STR_LEN             MAC_BYTES * 3 - 1       // Número de bytes que conforman la dirección MAC

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_err.h"
#include "esp_system.h"
#include "esp_log.h"

const char *nodo_get_mac();
void get_mac_str(const uint8_t *src, char *dest);

#endif
