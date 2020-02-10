#include <stdio.h>
#include "spifs.h"
#include "w25q32.h"
#include "misc.h"
#include "string.h"
#include <time.h>

void disp_name(uint8_t *buf, uint8_t max);
void disp_list(FileList *list);

int main(int argc, char **argv) {
    puts("spifs test application");
    w25q32_allocate();
    puts("w25q32 flash space allocated");
    w25q32_chip_erase();
    puts("chip erase finished (fill with 0xFF)");

    putchar('\n');

    File file;
    FileState fstate;
    Result result;
    FileList *list;

    uint8_t *buffer = (uint8_t *)malloc(sizeof(uint8_t) * 128);

    make_file(&file, "hello", "txt");
    make_fstate(&fstate, 2020, 2, 9);

    result = create_file(&file, fstate);
    puts("create file helle.txt");
    if(result == CREATE_FILEBLOCK_SUCCESS) {
        puts("CREATE_FILEBLOCK_SUCCESS");

        memset(buffer, 0xAB, sizeof(uint8_t) * 128);
        result = write_file(&file, buffer, 128);
        if(result == WRITE_FILE_SUCCESS) {
            puts("WRITE_FILE_SUCCESS");
        }

        memset(buffer, 0x00, sizeof(uint8_t) * 128);
        result = append_file(&file, buffer, 128);
        if(result == APPEND_FILE_SUCCESS) {
            puts("APPEND_FILE_SUCCESS");
        }

        result = append_finish(&file);
        if(result == APPEND_FILE_FINISH) {
            puts("APPEND_FILE_FINISH");
        }
    }
    putchar('\n');

    make_file(&file, "main", "java");
    result = create_file(&file, fstate);
    puts("create file main.java");
    if(result == CREATE_FILEBLOCK_SUCCESS) {
        puts("CREATE_FILEBLOCK_SUCCESS");

        memset(buffer, 0xBC, sizeof(uint8_t) * 128);
        result = write_file(&file, buffer, 128);
        if(result == WRITE_FILE_SUCCESS) {
            puts("WRITE_FILE_SUCCESS");
        }

        memset(buffer, 0xDE, sizeof(uint8_t) * 128);
        result = append_file(&file, buffer, 128);
        if(result == APPEND_FILE_SUCCESS) {
            puts("APPEND_FILE_SUCCESS");
        }

        result = append_finish(&file);
        if(result == APPEND_FILE_FINISH) {
            puts("APPEND_FILE_FINISH");
        }
    }

    list = list_file();
    disp_list(list);
    recycle_filelist(list);

    delete_file(&file);
    spifs_gc();

    uint8_t code = w25q32_output("I:\\ramdisk", "wb+", 40960);
    if(code) {
        puts("w25q32_output");
    }

    free(buffer);
    w25q32_destory();

    return 0;
}

void disp_name(uint8_t *buf, uint8_t max) {
    uint8_t i = 0;
    while(*buf != 0xFF && i < max) {
        putchar(*buf++);
        i++;
    }
}

void disp_list(FileList *list) {
    FileList *ptr = NULL;
    FileState fstate;
    puts("filelist:");
    while(list) {
        ptr = list->prev;
        putchar('\t');
        disp_name(list->File.filename, sizeof(list->File.filename));
        putchar('.');
        disp_name(list->File.extname, sizeof(list->File.extname));
        putchar('\n');

        putchar('\t');
        read_state(&(list->File), &fstate);
        printf("create time: %d-%d-%d\n", (fstate.year+2000), fstate.month, fstate.day);

        putchar('\t');
        printf("file state: 0x%x\n", fstate.state);

        putchar('\t');
        printf("block addr: 0x%x,cluster_addr: 0x%x,length: 0x%x\n",
               list->File.block, list->File.cluster, list->File.length);
        list = ptr;
    }
}
