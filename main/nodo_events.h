#ifndef NODO_EVENTS_H
#define NODO_EVENTS_H
#define WIFI_OK         (1UL << 0UL)   // => Indica que la estación ya estableció  una conexión WiFI con un AP
#define HTTP_OK         (1UL << 1UL)   // => Indica que la estación ya estableció una conexión con los servicios web
#define GATT_OK         (1UL << 2UL)   // => Indica que la estación ya estableció el servicio BLE-GATT para compartir datos
#define GATT_TASK_OK    (1UL << 2UL)   // => Indica que la estación ya lanzó el servicio para registrar los datos en el perfil GATT
#define TOKEN_OK        (1UL << 3UL)   // => Indica que se ha obtenido un token para comunicarse con los servicios web
#endif
