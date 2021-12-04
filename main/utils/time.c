#include "utils/time.h"

char *get_timestamp() {
    char buffer[128];
    char *timestamp_ptr;
    int timestamp_len;
    time_t now = time(NULL);
    struct tm *now_bd = localtime(&now);
    if ( now_bd == NULL) {
        return NULL;
    }
    if ( strftime(buffer, 64, "%F %T", now_bd) == 0 ) {
        return NULL;
    }
    timestamp_len = strlen(buffer);
    timestamp_ptr = (char *) calloc(timestamp_len + 1, sizeof(char));
    memcpy(timestamp_ptr, buffer, timestamp_len);
    *(timestamp_ptr + timestamp_len) = '\0';
    return timestamp_ptr;
}
