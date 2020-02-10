#ifndef __DISKIO_H__
#define __DISKIO_H__

#include "stdint.h"
#include "w25q32.h"
#include "spifs.h"

uint32_t disk_read(uint32_t address, uint8_t *buffer, uint32_t size);
uint8_t disk_write(uint32_t address, uint8_t *buffer, uint32_t size);

uint8_t chip_erase();
uint8_t sector_erase();

void write_fileblock(uint32_t addr, FileBlock *fb);
void clear_fileblock(uint8_t *baseAddr, uint32_t offset);

void write_value(uint32_t addr, uint32_t value, uint8_t bytes);
void write_fileblock_cluster(uint32_t fbaddr, uint32_t cluster);
void write_fileblock_length(uint32_t fbaddr, uint32_t length);
void write_fileblock_state(uint32_t fbaddr, uint8_t state);

#endif // __DISKIO_H__
