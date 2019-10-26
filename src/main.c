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
        printf("block addr:0x%x,cluster_addr:0x%x,length:0x%x\n",
               list->File.block, list->File.cluster, list->File.length);
        list = ptr;
    }
}

int main(int argc, char *argv[]) {
    puts("w25q32 spi flash emulator");

    w25q32_create();
    puts("w25q32_create");

    chip_erase();
    puts("chip_erase");

    File file;
    Result result;
    FileState state;

    make_file(&file, "hello", "txt");
    make_fstate(&state, 19, 10, 24);

    result = create_file(&file, state);
    printf("create_file:result: %d, addr: 0x%x\n", result, file.block);

    uint8_t *filebuffer = (uint8_t *)malloc(sizeof(uint8_t) * 5120);
    array_fill(filebuffer, 0xAA, 5120);

    result = write_file(&file, filebuffer, 5120, WRITE);
    printf("write_file_result: %d\n", result);
    printf("file.cluster: 0x%x,file.length: 0x%x\n", file.cluster, file.length);


    array_fill(filebuffer, 0xBB, 388);
    result = append_file(&file, filebuffer, 388);
    printf("append_file_result: %d\n", result);
    printf("file.length: 0x%x\n", file.length);

    array_fill(filebuffer, 0xEE, 165);
    result = append_file(&file, filebuffer, 165);
    printf("append_file_result: %d\n", result);
    printf("file.length: 0x%x\n", file.length);

    result = append_finish(&file);

    printf("append_finish: %d\n", result);


    free(filebuffer);


    printf("list of file:\n");
    FileList *list = list_file();
    disp_filelist(list);
    //delete_file(&(list->File));

    recycle_filelist(list);

    //system_gc();

    //printf("system_gc\n");

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
