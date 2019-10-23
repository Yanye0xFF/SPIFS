#ifndef __FILESYS_H__
#define __FILESYS_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

//24 bytes
typedef struct file_block {
    uint8_t filename[8];
    uint8_t extname[4];
    uint32_t cluster;
    uint32_t length;
    uint32_t state;
} FileBlock;

//4 bytes
typedef struct file_state {
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t state;
} FileState;

//24 bytes
typedef struct file {
    uint8_t filename[8];
    uint8_t extname[4];
    uint32_t block;
    uint32_t cluster;
    uint32_t length;
} File;

//32bytes(64bit), 28bytes(32bit)
typedef struct file_list {
    File File;
    struct file_list *prev;
} FileList;

typedef enum {
    CREATE_FILE_BLOCK_SUCCESS = 0,
    WRITE_FILE_SUCCESS,
    APPEND_FILE_SUCCESS,

    NO_FILE_BLOCK_SPACE,
    FILE_UNALLOCATED,
    NO_FILE_SECTOR_SPACE,
    FILE_CAN_NOT_APPEND
} Result;

typedef enum {
    WRITE = 0,
    APPEND
} WriteMethod;

#include "w25q32.h"
#include "misc.h"
#include "diskio.h"

Result create_file(File *file, FileState fstate);

Result write_file(File *file, uint8_t *buffer, uint32_t size, WriteMethod method);

void delete_file(File *file);

FileList *list_file();

void recycle_filelist(FileList *list);

void system_gc();
#endif
