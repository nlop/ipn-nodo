#include "measure_def.h"

/*
 * Función auxiliar para obtener la etiqueta de cada tipo de medición (utilizada para
 * generar entradas de JSON)
 */
const char *get_meas_type_str(enum measure_type_t type) {
    switch(type) {
        case TEMPERATURE:
            return "temperature";
        case HUMIDITY:
            return "humidity";
        case LIGHT:
            return "luminosity";
        case PH:
            return "ph";
        default:
            return NULL;
    }
}
