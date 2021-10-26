#ifndef NODO_EVENTS_H
#define NODO_EVENTS_H
#define WIFI_OK  (1UL << 0UL)   // => Indica que la estación ya estableció 
                                // una conexión WiFI con un AP
#define HTTP_OK  (1UL << 1UL)   // => Indica que la estación ya estableció
                                // una conexión con los servicios web
#define TOKEN_OK (1UL << 2UL)   // => Indica que se ha obtenido un token
                                // para comunicarse con los servicios web
#endif
