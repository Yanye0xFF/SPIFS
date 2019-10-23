#include "w25q32.h"

uint8_t *w25q32_buffer = NULL;

void w25q32_create() {
    if(w25q32_buffer == NULL) {
        w25q32_buffer = (uint8_t *)malloc(sizeof(uint8_t) * FLASH_SIZE);
    }
}

void w25q32_destory() {
    if(w25q32_buffer) free(w25q32_buffer);
}

uint8_t *w25q32_get_buffer() {
    return w25q32_buffer;
}


/**
 * 整个芯片擦除,擦除完成后为FF
 * W25Q16:25s
 * W25Q32:40s
 * W25Q64:40s
 * @return state register
 * */
uint8_t chip_erase() {
	for(uint32_t i = 0; i < FLASH_SIZE; i++) {
        *(w25q32_buffer + i) = 0xFF;
    }
	return 0x2;
}

/**
 * 扇区擦除 4KB, Tmin = 150ms
 * @param address 扇区起始地址
 * @return state register
 * */
uint8_t sector_erase(uint32_t address) {
    uint32_t start = address / SECTOR_SIZE;
    start *= SECTOR_SIZE;
    uint32_t end = start + SECTOR_SIZE;

    for(uint32_t i = start; i < end; i++) {
        *(w25q32_buffer + i) = 0xFF;
    }
	return 0x2;
}

/**
 * 块擦除 32KB
 * @param address 32K块起始地址
 * @return state register
 * */
uint8_t block_erase_32k(uint32_t address) {
	uint32_t start = address / BLOCK32K_SIZE;
    start *= BLOCK32K_SIZE;
    uint32_t end = start + BLOCK32K_SIZE;

    for(uint32_t i = start; i < end; i++) {
        *(w25q32_buffer + i) = 0xFF;
    }
	return 0x2;
}

/**
 * 块擦除 64KB
 * @param address 64K块起始地址
 * @return state register
 * */
uint8_t block_erase_64k(uint32_t address) {
	uint32_t start = address / BLOCK64K_SIZE;
    start *= BLOCK64K_SIZE;
    uint32_t end = start + BLOCK64K_SIZE;

    for(uint32_t i = start; i < end; i++) {
        *(w25q32_buffer + i) = 0xFF;
    }
	return 0x2;
}

uint32_t w25q32_read(uint8_t *buffer, uint32_t size, uint32_t address) {

	if(buffer == NULL || size <= 0) {
		return 0;
	}

    for(uint32_t i = 0; i < size; i++) {
        *(buffer + i) = *(w25q32_buffer + address + i);
    }

	return size;
}

/**
 * 写一页数据,最大256bytes
 * 由于超出后会回到初始地址覆盖数据,故内部限制MAX size = 256
 * @param buffer 写入数据缓冲区
 * @param size 实际写入数据长度
 * @param address 写入地址
 * */
uint8_t w25q32_write_page(uint8_t *buffer, uint32_t size, uint32_t address) {

	if(buffer == NULL || size <= 0) {
		return 0x00;
	}

	if(size > 256) {
		size = 256;
	}

    for(uint32_t i = 0; i < size; i++) {
        *(w25q32_buffer + address + i) = *(buffer + i);
    }

	return 0x2;
}

uint8_t w25q32_write_multipage(uint8_t *buffer, uint32_t size, uint32_t address) {

	if(buffer == NULL || size <= 0) {
		return 0x00;
	}

	if(size > 256) {
		size = 256;
	}

	return 0x2;
}

