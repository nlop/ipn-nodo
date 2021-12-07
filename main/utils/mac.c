#include "utils/mac.h"

char mac_nodo_str[32];
static bool mac_ready = false;

/*
 * Obtener el puntero a la cadena que representa la dirección MAC del
 * dispositivo Bluetooth
 */
const char *nodo_get_mac() {
    if (mac_ready == false) {
        uint8_t mac_nodo[6];
        ESP_ERROR_CHECK(esp_read_mac( mac_nodo, ESP_MAC_BT));
        get_mac_str(mac_nodo, mac_nodo_str);
        mac_ready = true;
    }
    return mac_nodo_str;
}

/*
 * Obtener la cadena que representa la dirección MAC en formato legible
 * a partir de los bytes almacenados en src, de los cuales se leen solamente
 * MAC_ADDR_LEN. La cadena es alojada en el puntero dest
 * Argumentos:
 *      src: Puntero donde estan almacenados los bytes de la dirección MAC
 *      dest: Puntero donde se almacenará la cadena creada
 */
void get_mac_str(const uint8_t *src, char *dest) {
    char tmp[8];
    for(uint8_t i = 0; i < MAC_ADDR_LEN; i++) {
        sprintf((char *) &tmp, "%02hX:", src[i]);
        memcpy(dest + i * 3, &tmp, 3 * sizeof(uint8_t));
    }
    /* Eliminar los dos puntos ':' del último bloque*/
    dest[MAC_ADDR_LEN * 3 - 1] = '\0';
}
