#ifndef WEB_HTTP_H
#define WEB_HTTP_H

#include "web/web_common.h"
#include "esp_log.h"
#include "esp_http_client.h"

#define HTTP_TAG            "HTTP"
#define HTTP_HOST           SERVER_URL
#define INIT_PATH           "/api/nodo_central/activate"
#if CONFIG_HTTP_PORT != 80
#define HTTP_PORT           CONFIG_HTTP_PORT
#endif

#define MAX_SEND_ATTEMPTS       6    // Numero máximo de intentos para intercambiar el token temporal

/** Estructuras **/
typedef struct token_ret_t {
    esp_err_t esp_status;
    uint8_t http_status;
    char *token;
    size_t token_len;
} token_ret_t;

token_ret_t http_send_token(uint8_t *token, const char *mac);

#endif 

