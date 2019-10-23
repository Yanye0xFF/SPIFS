#include <stdio.h>
#include "spifs.h"
#include "w25q32.h"
#include "misc.h"

void paint_name(uint8_t *buf, uint8_t max) {
    uint8_t i = 0;
    while(*buf != 0xFF) {
        if(i >= max) {
            break;
        }
        putchar(*buf++);
        i++;
    }
}

void disp_filelist(FileList *list) {
    FileList *ptr = NULL;
    while(list) {
        ptr = list->prev;
        printf("fname:");
        paint_name(list->File.filename, sizeof(list->File.filename));
        printf(",extname:");
        paint_name(list->File.extname, sizeof(list->File.extname));
        putchar('\n');
        printf("block:0x%x\n", list->File.block);
        printf("cluster:0x%x\n", list->File.cluster);
        printf("length:0x%x\n", list->File.length);

        list = ptr;
    }
}

void make_fstate(FileState *fstate) {
    fstate->day = 21;
    fstate->month = 10;
    fstate->year = 19;
    fstate->state = 0xFF;
}

void make_file(File *file, char *filename, char *extname) {
    file->block = 0x00;
    file->cluster = 0xFFFFFFFF;
    file->length = 0xFFFFFFFF;
    strcopy(filename, file->filename, strsize((uint8_t *)filename));
    strcopy(extname, file->extname, strsize((uint8_t *)extname));
}

int main(int argc, char *argv[]) {
    puts("w25q32 spi flash emulator");

    w25q32_create();
    puts("w25q32_create");

    chip_erase();
    puts("chip_erase");

    File file;
    make_file(&file, "hello", "txt");

    FileState state;
    make_fstate(&state);

    Result result;

    result = create_file(&file, state);
    printf("result: %d, addr: 0x%x\n", result, file.block);

    uint8_t *filebuffer = (uint8_t *)malloc(sizeof(uint8_t) * 352);
    array_fill(filebuffer, 0xAA, 32);

    result = write_file(&file, filebuffer, 32, WRITE);
    printf("write_file_result: %d\n", result);
    printf("file.cluster: 0x%x\n", file.cluster);
    printf("file.length: 0x%x\n", file.length);

    array_fill(filebuffer, 0xBB, 317);
    result = write_file(&file, filebuffer, 317, APPEND);
    printf("write_file_result: %d\n", result);
    printf("file.cluster: 0x%x\n", file.cluster);
    printf("file.length: 0x%x\n", file.length);


    array_fill(filebuffer, 0xCC, 12);
    result = write_file(&file, filebuffer, 12, APPEND);
    printf("write_file_result: %d\n", result);
    printf("file.cluster: 0x%x\n", file.cluster);
    printf("file.length: 0x%x\n", file.length);

    free(filebuffer);

    /*
    printf("list of file:\n");
    FileList *list = list_file();
    disp_filelist(list);
    delete_file(&(list->File));
    recycle_filelist(list);
    */
    if(1) {
        FILE *out = fopen("ramdisk", "wb" );
        uint8_t *buff = w25q32_get_buffer();
        fwrite(buff, sizeof(uint8_t), 40960, out);
        fclose(out);
        puts("fwrite ramdisk");
    }

    w25q32_destory();
    puts("w25q32_destory");
    return 0;
}
