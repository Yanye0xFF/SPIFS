#ifndef __DISKIO_H__
#define __DISKIO_H__

#include "stdint.h"
#include "spifs.h"

uint8_t sector_isempty(uint32_t sec_id);
uint8_t mark_sector_inuse(uint32_t sec_id);
void write_fileblock(uint32_t addr, FileBlock *fb);

void write_value(uint32_t addr, uint32_t value, uint8_t bytes);

void write_fileblock_cluster(uint32_t fbaddr, uint32_t cluster);
void write_fileblock_length(uint32_t fbaddr, uint32_t length);
void write_fileblock_state(uint32_t fbaddr, uint8_t state);

#endif // __DISKIO_H__
