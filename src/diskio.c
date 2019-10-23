#include "diskio.h"

/*
检查扇区使用记录表
记录表占用扇区0的低512字节
总计可表示4096扇区使用情况
bit0~bit3 用于标识扇区0~扇区3文件块使用情况
1: 该扇区还有可用文件块空间
0: 该扇区已满
其余记录扇区已占用/未占用
1: 未占用
0: 已使用
@param sec_id 扇区逻辑编号
*/
uint8_t sector_isempty(uint32_t sec_id) {
    uint8_t mem = 0xFF;
    uint32_t idx = (sec_id / 8) * 8, pos = sec_id % 8;
    w25q32_read(&mem, 1, idx);
    return (mem >> pos) & 0x1;
}

/*
向扇区使用记录表标注扇区被使用
@param sec_id 扇区逻辑编号
*/
uint8_t mark_sector_inuse(uint32_t sec_id) {
    uint8_t mem = 0xFF;
    uint8_t res = 0x00;
    uint32_t idx = (sec_id / 8) * 8, pos = sec_id % 8;
    w25q32_read(&mem, 1, idx);
    if(((mem >> pos) & 0x1) == 0) {
        return 0;
    }
    mem &= (~(0x1 << pos));
    res = w25q32_write_page(&mem, 1, idx);
    return (res == 0x2);
}

/*
写文件块记录
addr为物理地址，应在：[扇区0 0x200 ~ 扇区3 0x3FFF]
@param addr 写入的物理地址
@param *fb 文件结构块指针
*/
void write_fileblock(uint32_t addr, FileBlock *fb) {
    uint8_t *slot_buffer = (uint8_t *)fb;
    w25q32_write_page(slot_buffer, 24, addr);
}

void write_value(uint32_t addr, uint32_t value, uint8_t bytes) {
    uint8_t buffer[bytes];
    for(uint8_t i = 0; i < bytes; i++) {
        buffer[i] = (value >> (i * 8)) & 0xFF;
    }
    w25q32_write_page(buffer, bytes, addr);
}

void write_fileblock_cluster(uint32_t fbaddr, uint32_t cluster) {
    write_value(fbaddr + 12, cluster, 4);
}

void write_fileblock_length(uint32_t fbaddr, uint32_t length) {
    write_value(fbaddr + 16, length, 4);
}

void write_fileblock_state(uint32_t fbaddr, uint8_t state) {
    write_value(fbaddr + 23, state, 1);
}
