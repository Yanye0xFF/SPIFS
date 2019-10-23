#ifndef __W25Q32_H__
#define __W25Q32_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 256
#define SECTOR_SIZE 4096
#define BLOCK32K_SIZE 32768
#define BLOCK64K_SIZE 65536
#define FLASH_SIZE 4194304

#define PAGE_NUM 16384
#define SECTOR_NUM 1024
#define BLOCK32K_NUM 128
#define BLOCK64K_NUM 64

extern uint8_t *w25q32_buffer;

void w25q32_create();
void w25q32_destory();
uint8_t *w25q32_get_buffer();

uint32_t w25q32_read(uint8_t *buffer, uint32_t size, uint32_t address);

uint8_t w25q32_write_page(uint8_t *buffer, uint32_t size, uint32_t address);
uint8_t w25q32_write_multipage(uint8_t *buffer, uint32_t size, uint32_t address);

uint8_t sector_erase(uint32_t address);
uint8_t block_erase_32k(uint32_t address);
uint8_t block_erase_64k(uint32_t address);
uint8_t chip_erase();

#endif
