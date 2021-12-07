#ifndef MEASURE_DEF_H
#define MEASURE_DEF_H
#include <stdint.h>
#include <stdio.h>

enum measure_type_t { TEMPERATURE, HUMIDITY, PH, LIGHT };

typedef struct measure_t {
    enum measure_type_t type; 
    uint16_t value;
} measure_t;

typedef struct measure_vector_t {
    uint8_t len;
    struct measure_t *data;
} measure_vector_t;

const char *get_meas_type_str(enum measure_type_t type);
#endif
