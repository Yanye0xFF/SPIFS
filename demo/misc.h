#ifndef __MISC_H__
#define __MISC_H__

#include "stdint.h"

void array_fill(uint8_t *buffer, uint8_t ch, uint32_t size);
void array_copy(uint8_t *from, uint8_t *to, uint32_t size);

void copy_filename(char *src, uint8_t *target, uint32_t length, uint32_t max);
uint8_t comp_filename(uint8_t *fname, char *str, uint8_t str_size);

#endif // __MISC_H__
