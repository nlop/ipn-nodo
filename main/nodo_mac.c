#include "nodo_mac.h"

char mac_nodo_str[32];
uint8_t mac_ready = 0;
void get_mac_str(uint8_t *src, char *dest);

/*
 * Obtener el puntero a la cadena que representa la dirección MAC del
 * dispositivo Bluetooth
 */
const char *nodo_get_mac() {
    if (mac_ready == 0) {
        uint8_t mac_nodo[6];
        ESP_ERROR_CHECK(esp_read_mac( mac_nodo, ESP_MAC_BT));
        get_mac_str((uint8_t *) mac_nodo, mac_nodo_str);
        mac_ready = 1;
    }
    return mac_nodo_str;
}

/*
 * Obtener la cadena que representa la dirección MAC en formato legible
 * a partir de los bytes almacenados en src, de los cuales se leen solamente
 * MAC_BYTES. La cadena es alojada en el puntero dest
 * Argumentos:
 *      src: Puntero donde estan almacenados los bytes de la dirección MAC
 *      dest: Puntero donde se almacenará la cadena creada
 */
void get_mac_str(uint8_t *src, char *dest) {
    char tmp[8];
    for(uint8_t i = 0; i < MAC_BYTES; i++) {
        sprintf((char *) &tmp, "%02hX:", src[i]);
        memcpy(dest + i*3, &tmp, 3 * sizeof(uint8_t));
    }
    dest[MAC_BYTES * 3 - 1] = '\0';
}
