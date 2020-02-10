#include "misc.h"

void array_fill(uint8_t *buffer, uint8_t ch, uint32_t size) {
    for(uint32_t i = 0; i < size; i++) {
        *(buffer + i) = ch;
    }
}

void array_copy(uint8_t *from, uint8_t *to, uint32_t size) {
    for(uint32_t i = 0; i < size; i++) {
        *(to + i) = *(from + i);
    }
}

void copy_filename(char *src, uint8_t *target, uint32_t length, uint32_t max) {
    for(uint32_t i = 0; i < max; i++) {
        *(target + i) = (i < length) ? (*(src + i)) : 0xFF;
    }
}

uint8_t comp_filename(uint8_t *fname, char *str, uint8_t str_size) {
    for(uint8_t i = 0; i < str_size; ++i) {
        if(*(fname + i) != *(str +i)) {
            return 0;
        }
    }
    return 1;
}
