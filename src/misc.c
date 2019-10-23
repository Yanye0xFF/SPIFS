#include "misc.h"

void array_fill(uint8_t *buffer, uint8_t ch, uint32_t size) {
    while(--size) {
         *(buffer + size) = ch;
    }
    *(buffer + size) = ch;
}

void array_copy(uint8_t *from, uint8_t *to, uint32_t size) {
    for(uint32_t i = 0; i < size; i++) {
        *(to + i) = *(from + i);
    }
}

uint32_t strsize(uint8_t *str) {
    uint32_t size = 0;
    while(*str++) {
        size++;
    }
    return size;
}

void strcopy(char *from, uint8_t *to, uint32_t size) {
    for(uint32_t i = 0; i < size; i++) {
        *(to + i) = *(from + i);
    }
}
