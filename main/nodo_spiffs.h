#ifndef NODO_SPIFFS_H
#define NODO_SPIFFS_H

#include <stdio.h>
#include "esp_spiffs.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SPIFFS_TAG          "NODO_SPIFFS"
#define GATTC_DB_PATH       "/spiffs/gattc_db.bin"
#define ENTRY_LEN           6

/* Estructuras de datos */
typedef struct db_entries_t {
    uint8_t len;
    uint8_t **data;
} db_entries_t;

/** Funciones **/
int spiffs_mount(void);
int spiffs_init(void);
int spiffs_add_entry(uint8_t *dev_addr);
int spiffs_delete_entry(uint8_t index);
int spiffs_get_all(db_entries_t *ret);
void db_entries_free(db_entries_t entries) ;
void spiffs_umount(void);
int spiffs_mounted(void);

#endif
