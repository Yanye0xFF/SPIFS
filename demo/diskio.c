#include "diskio.h"

/**
 * 读取
 * @param address 地址
 * @param buffer 读入缓冲区
 * @param size 读取大小(字节)
 * @return 实际读取大小(字节)
 **/
uint32_t disk_read(uint32_t address, uint8_t *buffer, uint32_t size) {
    return w25q32_read(address, buffer, size);
}

/**
 * 写入
 * @param address 地址
 * @param buffer 读入缓冲区
 * @param size 写入大小(字节)
 * @return 0x2: 写入成功
 * */
uint8_t disk_write(uint32_t address, uint8_t *buffer, uint32_t size) {
    return w25q32_write_page(address, buffer, size);
}

/**
 * 扇区擦除
 * @param address 扇区首地址
 * @return 0x2: 擦除成功
 * */
uint8_t chip_erase() {
    return w25q32_chip_erase();
}

/**
 * 扇区擦除
 * @param address 扇区首地址
 * @return 0x2: 擦除成功
 * */
uint8_t sector_erase(uint32_t address) {
    return w25q32_sector_erase(address);
}

/**
 * 写文件块记录
 * @param addr 物理地址
 * @param *fb 文件结构块指针
 * */
void write_fileblock(uint32_t addr, FileBlock *fb) {
    uint8_t *slot_buffer = (uint8_t *)fb;
    w25q32_write_page(addr, slot_buffer, 24);
}

/**
 * 擦除文件块记录
 * @param baseAddr 基址
 * @param offset 偏移量
 * */
void clear_fileblock(uint8_t *baseAddr, uint32_t offset) {
    for(uint32_t i = 0; i < FILEBLOCK_SIZE; i++) {
        *(baseAddr + offset + i) = 0xFF;
    }
}

/**
 * 在指定地址写值
 * @param addr 物理地址
 * @param value 写入数据
 * @param bytes 字节数
 * */
void write_value(uint32_t addr, uint32_t value, uint8_t bytes) {
    uint8_t buffer[4];
    if(bytes > 4) return;
    for(uint8_t i = 0; i < bytes; i++) {
        buffer[i] = (value >> (i << 3)) & 0xFF;
    }
    w25q32_write_page(addr, buffer, bytes);
}

/**
 * 写文件块数据区首簇地址
 * @param fbaddr 文件块地址
 * @param cluster 首簇地址
 * */
void write_fileblock_cluster(uint32_t fbaddr, uint32_t cluster) {
    write_value(fbaddr + 12, cluster, 4);
}

/**
 * 写文件块文件长度
 * @param fbaddr 文件块地址
 * @param length 文件长度
 * */
void write_fileblock_length(uint32_t fbaddr, uint32_t length) {
    write_value(fbaddr + 16, length, 4);
}

/**
 * 写文件块文件状态字段
 * @param fbaddr 文件块地址
 * @param state 文件状态字段
 * */
void write_fileblock_state(uint32_t fbaddr, uint8_t state) {
    write_value(fbaddr + 23, state, 1);
}
