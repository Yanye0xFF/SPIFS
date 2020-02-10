#ifndef __FILESYS_H__
#define __FILESYS_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "string.h"

// 4字节对齐

// 文件索引块结构(24字节)
typedef struct file_block {
    uint8_t filename[8]; // 文件名
    uint8_t extname[4]; // 拓展名
    uint32_t cluster;  // 首簇地址
    uint32_t length;  // 文件大小
    uint32_t state;  // 文件状态
} FileBlock;

// 文件状态字结构(4字节)
typedef struct file_state {
    uint8_t day;      // 创建日期
    uint8_t month;   // 创建月份
    uint8_t year;   // 创建年份,以2000年为基准,记录经过年数
    uint8_t state; // 文件状态字
} FileState;

// 文件信息结构(24字节)
typedef struct file {
    uint8_t filename[8]; // 文件名
    uint8_t extname[4]; // 拓展名
    uint32_t block;    // 文件索引记录地址
    uint32_t cluster; // 文件内容起始扇区地址
    uint32_t length; // 文件大小
} File;

// 文件信息链表
// 32bytes(64bit), 28bytes(32bit)
typedef struct file_list {
    File File;
    struct file_list *prev;
} FileList;

typedef enum {
    CREATE_FILEBLOCK_SUCCESS = 0,
    WRITE_FILE_SUCCESS,

    APPEND_FILE_SUCCESS,
    APPEND_FILE_FINISH,

    NO_FILEBLOCK_SPACE,
    NO_SECTOR_SPACE,

    FILE_UNALLOCATED,
    FILE_CANNOT_APPEND
} Result;

#include "misc.h"
#include "diskio.h"

// 文件索引起始扇区号
#define FB_SECTOR_INIT 0
// 文件索引结束扇区号
#define FB_SECTOR_END 4
// 文件索引占用扇区范围(FB_SECTOR_INIT ~ FB_SECTOR_END - 1)

// 文件索引占用空间大小(字节)
#define FILEBLOCK_SIZE 24
// 文件名+拓展名占用空间大小(字节)
#define FILENAME_FULLSIZE 12

// Flash扇区总数
#define SECTOR_SUM 1024
// Flash页总数
#define PAGE_SUM 16384

// Flash页大小(字节)
#define PAGE_SIZE 256
// Flash扇区大小(字节)
#define SECTOR_SIZE 4096
// Flash大小(字节)
#define FLASH_SIZE 4194304
// 扇区内数据域大小(字节)
#define DATA_AREA_SIZE 4090
// 扇区标记位大小(字节)
#define SECTOR_STATE_SIZE 2

void make_file(File *file, char *filename, char *extname);
void make_fstate(FileState *fstate, uint32_t year, uint8_t month, uint8_t day);

Result create_file(File *file, FileState fstate);
Result write_file(File *file, uint8_t *buffer, uint32_t size);
Result append_file(File *file, uint8_t *buffer, uint32_t size);
Result append_finish(File *file);

uint8_t open_file(File *file, char *filename, char *extname);
uint8_t read_state(File *file, FileState *state);
uint8_t read_file(File *file, uint8_t *buffer, uint32_t offset, uint32_t size);

void delete_file(File *file);
void spifs_gc();

FileList *list_file();
void recycle_filelist(FileList *list);

#endif
