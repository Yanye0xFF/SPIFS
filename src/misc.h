#ifndef __MISC_H__
#define __MISC_H__

#include "stdint.h"

void array_fill(uint8_t *buffer, uint8_t ch, uint32_t size);
void array_copy(uint8_t *from, uint8_t *to, uint32_t size);

uint32_t strsize(uint8_t *str);
void strcopy(char *from, uint8_t *to, uint32_t size);

#endif // __MISC_H__
