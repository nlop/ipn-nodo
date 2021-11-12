#include "nodo_spiffs.h"

/*
 * Archivo para almacenar las estaciones asociadas a un sinknode
 *
 * ======> Estructura del archivo
 *
 * +-------------+-------------+-------------+------------+
 * |    MAC[0]   |      ...    |   MAC[n-1]  | Entradas(n)|
 * +-------------+-------------+-------------+------------+
 * | uint_8_t[6] |      ...    | uint_8_t[6] |  uint_8_t  |
 *
 */

/* Configuración del sistema de archivos SPIFFS */
esp_vfs_spiffs_conf_t conf = {
  .base_path = "/spiffs",
  .partition_label = NULL,          /* Utilizar la partición por defecto */
  .max_files = 5,
  .format_if_mount_failed = true
};

/*
 * Montar la partición SPIFFS y mostrar sus datos
 */
int spiffs_mount(void) {
    ESP_LOGI(SPIFFS_TAG, "%s: Inicializando SPIFFS...", __func__);

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(SPIFFS_TAG, "%s: Fallo montando/formateando!", __func__);
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(SPIFFS_TAG, "%s: No se encontro la partición SPIFFS!", __func__);
        } else {
            ESP_LOGE(SPIFFS_TAG, "%s: Fallo inicializando [code %s]", 
                    __func__, esp_err_to_name(ret));
        }
        return -1;
    }
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(SPIFFS_TAG, "%s: Error obteniendo información sobre la partición [code %s]", 
                __func__, esp_err_to_name(ret));
        return -1;
    } else {
        ESP_LOGI(SPIFFS_TAG, "%s: Tamaño partición: total: %zu, usada: %zu", __func__, total, used);
    }
    return 0;
}

/* Inicializar por primera vez el sistema de archivos SPIFFS y realizar lo
 * siguiente:
 *  - Montar el FS
 *  - Abrir GATTC_DB_PATH
 *  - Escribir el #entradas (0)
 */
int spiffs_init(void) {
    ESP_LOGI(SPIFFS_TAG, "%s", __func__);
    if ( spiffs_mount() != 0 ) {
        ESP_LOGE(SPIFFS_TAG, "Error iniciando SPIFFS!");
        return -1;
    }
    FILE *gattc_db = fopen(GATTC_DB_PATH, "w+");
    if (gattc_db == NULL) {
        ESP_LOGE(SPIFFS_TAG, "%s: Error creando archivo %s!", __func__, GATTC_DB_PATH);
    }
    /* Iniciar con 0 entradas */
    int ret = fputc(0x0, gattc_db);
    if ( ret == EOF ) {
        ESP_LOGE(SPIFFS_TAG, "%s: Error escribiendo en %s!", __func__, GATTC_DB_PATH);
        return -1;
    }
    fputc(EOF, gattc_db);
    fclose(gattc_db);
    return 0;
}

/*
 * Agrear una entrada a la base de datos del servidores para GATTC
 *
 *  Argumentos:
 *      dev_addr : Arreglo de 6 bytes con la dirección MAC del servidor GATT
 *  Regresa:
 *      Entero 0 cuando la operación se realizó exitosamente, -1 en cualquier
 *      otro caso
 */
int spiffs_add_entry(uint8_t *dev_addr) {
    ESP_LOGI(SPIFFS_TAG, "%s", __func__);
    uint8_t num_entries;
    FILE *gattc_db = fopen(GATTC_DB_PATH, "r+");
    if (gattc_db == NULL) {
        ESP_LOGE(SPIFFS_TAG, "%s: Error abriendo archivo %s!", __func__, GATTC_DB_PATH);
        return -1;
    }
    fseek(gattc_db, -2L, SEEK_END);
    int ret = fread(&num_entries, sizeof(uint8_t), 1, gattc_db);
    if ( ret != 1 ) {
        ESP_LOGE(SPIFFS_TAG, "%s| Error obteniendo #entradas!", __func__);
        return -1;
    }
    ESP_LOGI(SPIFFS_TAG, "#entradas = %02d", num_entries);
    num_entries += 1;
    fseek(gattc_db, -1L, SEEK_CUR);
    ret = fwrite(dev_addr, sizeof(uint8_t), ENTRY_LEN, gattc_db);
    if (ret != ENTRY_LEN) {
        ESP_LOGE(SPIFFS_TAG, "%s: Error escribiendo entrada!", __func__);
        return -1;
    }
    ret = fwrite(&num_entries, sizeof(uint8_t), 1, gattc_db);
    if ( ret != 1 ) {
        ESP_LOGE(SPIFFS_TAG, "%s| Error actualizando #entradas!", __func__);
        return -1;
    }
    fputc(EOF, gattc_db);
    fclose(gattc_db);
    return 0;
}

/* Eliminar una entrada de la base de datos de servidores para GATTC
 *
 * Argumentos:
 *      index : Índice de la entrada según según se encuentra en el retorno de
 *      la función spiffs_get_all
 *
 * Regresa:
 *      Entero 0 cuando la operación se realizó exitosamente, -1 en cualquier
 *      otro caso
 *
 * TODO: Reacomodar las entradas cuando se borra cualquier elemento diferente
 * al que se encuentra al final
 */
int spiffs_delete_entry(uint8_t index) {
    ESP_LOGI(SPIFFS_TAG, "%s", __func__);
    uint8_t num_entries = 0;
    FILE *gattc_db = fopen(GATTC_DB_PATH, "r+");
    if (gattc_db == NULL) {
        ESP_LOGE(SPIFFS_TAG, "%s: Error abriendo archivo %s!", __func__, GATTC_DB_PATH);
        return -1;
    }
    fseek(gattc_db, -2L, SEEK_END);
    /* Obtener #entradas */
    int ret = fread(&num_entries, sizeof(uint8_t), 1, gattc_db);
    if ( ret != 1 ) {
        ESP_LOGE(SPIFFS_TAG, "%s| Error obteniendo #entradas!", __func__);
        return -1;
    }
    /* Colocar el puntero de posición en la entrada[index] */
    if ( index > num_entries || (fseek(gattc_db, num_entries * ENTRY_LEN, SEEK_SET) == -1) ) {
        ESP_LOGE(SPIFFS_TAG, "%s| Error buscando entrada!", __func__); 
        return -1;  
    }
    for(uint8_t i = 0; i < ENTRY_LEN ; i++) {
        if ( fputc(0x0, gattc_db) == EOF ) {
            ESP_LOGE(SPIFFS_TAG, "%s| Error borrando entrada!", __func__);
        }
    }
    /* Actualizar #entradas */
    fseek(gattc_db, -2L, SEEK_END);
    num_entries += 1;
    ret = fwrite(&num_entries, sizeof(uint8_t), 1, gattc_db);
    if ( ret != 1 ) {
        ESP_LOGE(SPIFFS_TAG, "%s| Error actualizando #entradas!", __func__);
        return -1;
    }
    // fputc(EOF, gattc_db);
    fclose(gattc_db);
    return 0;
}

/* Obtener todas las entradas de la base de datos de servidores para GATTC
 *
 * Cada entrada corresponde a los bytes de la dirección MAC de cada servidor.
 *
 * Argumentos:
 *      ret : Puntero a una estructura db_entries_t donde se almacenará el
 *      arreglo de cadenas de bytes, así como el número de estas.
 * Regresa:
 *      Entero 0 cuando la operación se realizó exitosamente, -1 en cualquier
 *      otro caso. Adicionalmente, el número de entradas en ret es 0, y el
 *      puntero a los arreglos es NULL.
 *
 */
int spiffs_get_all(db_entries_t *ret) {
    ESP_LOGI(SPIFFS_TAG, "%s", __func__);
    FILE *gattc_db = fopen(GATTC_DB_PATH, "r");
    uint8_t num_entries = 0;
    uint8_t **entries;
    if (gattc_db == NULL) {
        ESP_LOGE(SPIFFS_TAG, "%s: Error abriendo archivo %s!", __func__, GATTC_DB_PATH);
        return -1;
    }
    fseek(gattc_db, -2L, SEEK_END);
    /* Obtener #entradas */
    int read_ret = fread(&num_entries, sizeof(uint8_t), 1, gattc_db);
    if ( read_ret != 1 ) {
        ESP_LOGE(SPIFFS_TAG, "%s| Error obteniendo #entradas!", __func__);
        return -1;
    }
    if ( num_entries == 0 ) {
        ESP_LOGE(SPIFFS_TAG, "%s| DB vacía!", __func__);
        return -1;
    }
    ESP_LOGI(SPIFFS_TAG, "#entradas = %02d", num_entries);
    if ( (entries = (uint8_t **) calloc(num_entries, sizeof(uint8_t *))) == NULL) {
        ESP_LOGE(SPIFFS_TAG, "%s| Error asignando memoria!", __func__);
        return -1;
    }
    fseek(gattc_db, 0L, SEEK_SET);
    for(uint8_t i = 0; i < num_entries; i++ ) {
        uint8_t *entry_tmp;
        if ( (entry_tmp = (uint8_t *) calloc(ENTRY_LEN, sizeof(uint8_t))) == NULL ) {
            ESP_LOGE(SPIFFS_TAG, "%s| Error asignando memoria!", __func__);
            /* Liberar la memoria asignada antes del error */
            for(i -= 1; i > 0; --i, free(entries[i]));
            free(entries);
            return -1;
        } else {
            fread(entry_tmp, sizeof(uint8_t), ENTRY_LEN, gattc_db);
            entries[i] = entry_tmp;
        }
    }
    fclose(gattc_db);
    ret->data = entries;
    ret->len = num_entries;
    return 0;
}

/*
 * Libera los recursos obtenidos a traves de spiffs_get_all
 *
 * Al accionar esta función, se liberan todas las cadenas del campo 'data',
 * utilizando como índice el campo 'len'
 *
 * Argumento:
 *      entries : Estructura de datos que contiene los punteros que serán
 *      liberados.
 */
void db_entries_free(db_entries_t entries) {
    ESP_LOGI(SPIFFS_TAG, "%s", __func__);
    for(uint8_t i = 0; i < entries.len; free(entries.data[i]), i++);
    free(entries.data);
}

/*
 * Desmontar el sistema de archivos SPIFFS
 */
void spiffs_umount(void) {
    ESP_LOGI(SPIFFS_TAG, "%s", __func__);
    esp_vfs_spiffs_unregister(conf.partition_label);
}

/*
 * Obtener el estado del sistema de archivos
 *
 * Regresa:
 *     0 en caso de estar montado, de lo contrario, -1.
 */
int spiffs_mounted(void) {
    return ( esp_spiffs_mounted(NULL) == true ) ? 0 : -1 ; 
}
