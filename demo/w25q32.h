#ifndef __W25Q32_H__
#define __W25Q32_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern uint8_t *w25q32_buffer;

void w25q32_allocate();
void w25q32_destory();
uint8_t * w25q32_getbuffer();
uint8_t w25q32_output(const char *filePath, const char *mode, uint32_t size);

uint32_t w25q32_read(uint32_t address, uint8_t *buffer, uint32_t size);
uint8_t w25q32_write_page(uint32_t address, uint8_t *buffer, uint32_t size);
uint8_t w25q32_write_multipage(uint32_t address, uint8_t *buffer, uint32_t size);

uint8_t w25q32_chip_erase();
uint8_t w25q32_sector_erase(uint32_t address);
uint8_t w25q32_block_erase_32k(uint32_t address);
uint8_t w25q32_block_erase_64k(uint32_t address);

#endif
