#include "spifs.h"

/*
簇大小 == 扇区大小 == 4KB
sector0 ~ sector 3 扇区占用索引表,文件索引表

sector0: 0x000 ~ 0x1FF 扇区占用索引表,共512字节
可表示4096个簇占用情况,即最大支持flash容量为16MB

sector0: 0x200 ~ sector3 : 0x3FFF 文件索引表
每个文件索引占24字节,共可支持659个文件

文件簇 = 4KB, 其中数据区4092字节,最后4字节为下一簇号指针,FFFFFFFF表示文件结束
(文件簇末尾4字节还可使用物理地址,具体取决于实现)
*/

/*
0扇区 偏移512字节
*/

void mark_bit(uint8_t *buffer, uint32_t sec_id) {
    uint32_t idx = (sec_id / 8) * 8, pos = sec_id % 8;
    *(buffer + idx) |= (0x1 << pos);
}

uint8_t check_bit(uint8_t *buffer, uint32_t sec_id) {
    uint32_t idx = (sec_id / 8) * 8, pos = sec_id % 8;
    return ((*(buffer + idx) >> pos) & 0x1);
}

void clear_fileblock(uint8_t *buffer, uint32_t offset) {
    for(uint32_t i = 0; i < 24; i++) {
        *(buffer + offset + i) = 0xFF;
    }
}

const uint32_t addr_tab[4] = {0x0200, 0x1000, 0x2000, 0x3000};
const uint32_t addr_idx[4] = {0x0000, 0x1000, 0x2000, 0x3000};

Result create_file(File *file, FileState fstate) {

    FileBlock *fb = NULL;
    uint8_t has_gc = 0;
    uint32_t idx = 0, addr_start, addr_end, temp = 0x00, count = 0x00;

    uint8_t *slot_buffer = (uint8_t *)malloc(sizeof(uint8_t) * 24);

    //check sector usage table
    FIND_SECTOR:
    for(idx = 0; idx < 4; idx++) {
        if(sector_isempty(idx)) {
            break;
        }
    }

    if(idx >= 4) {
        if(has_gc == 0) {
            system_gc();
            has_gc = 1;
            goto FIND_SECTOR;
        }
        return NO_FILE_BLOCK_SPACE;
    }

    addr_start = addr_tab[idx];
    addr_end = 0x1000 * (idx + 1) - 24;

    for(; addr_start < addr_end; addr_start += 24) {
        w25q32_read(slot_buffer, 24, addr_start);
        fb = (FileBlock *)slot_buffer;
        if(fb->state == 0xFFFFFFFF) {
            temp = (temp == 0x00) ? addr_start : temp;
            count++;
        }
    }
    addr_start = temp;

    if(count <= 1) {
         mark_sector_inuse(idx);
    }

    array_fill(slot_buffer, 0xFF, 24);
    fb = (FileBlock *)slot_buffer;

    array_copy(file->filename, fb->filename, strsize(file->filename));
    array_copy(file->extname, fb->extname, strsize(file->extname));
    fb->state = *(uint32_t *)&fstate;

    write_fileblock(addr_start, fb);

    file->block = addr_start;
    file->cluster = fb->cluster;
    file->length = fb->length;

    free(slot_buffer);

    return CREATE_FILE_BLOCK_SUCCESS;
}

Result write_file(File *file, uint8_t *buffer, uint32_t size, WriteMethod method) {

    if(file->block == 0x00) return FILE_UNALLOCATED;
    uint32_t pos = 0, sectors, *sector_list;

    if(method == WRITE) {
        //calc sector amounts
        sectors = size / 4092;
        if((size % 4092) != 0) {
            sectors += 1;
        }
        //find available sectors
        sector_list = (uint32_t *)malloc(sizeof(uint32_t) * sectors);
        //start position is 4
        for(uint32_t i = 4; i < 4096; i++) {
            if(pos >= sectors) break;
            if(sector_isempty(i)) {
                *(sector_list + pos) = i * 0x1000;
                pos++;
            }
        }
        //check available sectors
        if(pos != sectors) return NO_FILE_SECTOR_SPACE;

        for(uint32_t i = 0; i < sectors; i++) {
            mark_sector_inuse(*(sector_list + i) / 0x1000);
        }

        pos = 0;
        uint32_t write_size, write_addr, addr_pos;

        write_fileblock_cluster(file->block, *(sector_list + 0));
        write_fileblock_length(file->block, size);
        file->cluster = *(sector_list + 0);
        file->length = size;
        //sector loop
        for(uint32_t i = 0; i < sectors; i++) {
            write_addr = *(sector_list + i);
            write_size = 0;
            addr_pos = 0;
            //page loop
            while(size) {
                if(addr_pos >= 4092) {
                    write_value((write_addr + addr_pos), *(sector_list + i + 1), 4);
                    break;
                }
                write_size = (size >= 256) ? 256 : (size % 256);
                if((addr_pos + write_size) > 4092) {
                    write_size = 4092 - addr_pos;
                }
                w25q32_write_page((buffer + pos), write_size, (write_addr + addr_pos));
                pos += write_size;
                size -= write_size;
                addr_pos += write_size;
            }
        }

        free(sector_list);
        return WRITE_FILE_SUCCESS;

    }else if(method == APPEND) {

        if(file->cluster == 0xFFFFFFFF) return FILE_CAN_NOT_APPEND;
        //read old file
        uint32_t next_addr = file->cluster, temp;
        //计算文件结束位置
        pos = file->length % 4092;
        //文件长度超过一扇区，遍历找到最后一个扇区首地址
        if(file->length >= 4092) {
            sectors = file->length / 4092;
            for(uint32_t i = 0; i < sectors; i++) {
                w25q32_read((uint8_t *)&temp, 4, (next_addr + 4092));
                next_addr = temp;
            }
        }
        //计算该扇区空余空间
        uint32_t left_size = (next_addr + 4092) - (next_addr + pos);
        //追加模式写新内容起始地址
        uint32_t write_addr = next_addr + pos;
        printf("write_addr:0x%x,left_size:%d\n",write_addr,left_size);

        pos = 0;
        uint32_t write_size = 0x00, addr_pos = 0x00;
        if(left_size >= size) {
            //剩余空间足够写追加内容
            temp = size;
            while(temp) {
                write_size = (temp >= 256) ? 256 : (temp % 256);
                w25q32_write_page((buffer + pos), write_size, (write_addr + addr_pos));
                pos += write_size;
                temp -= write_size;
                addr_pos += write_size;
            }
            //确定文件块记录所属扇区
            pos = file->block / 0x1000;
            uint8_t *sector_buffer = (uint8_t *)malloc(sizeof(uint8_t) * 4096);
            addr_pos = (pos == 0) ? 0x00 : addr_tab[pos];
            w25q32_read(sector_buffer, 4096, addr_pos);
            printf("addr_pos:0x%x\n", addr_pos);
            //计算新的文件大小
            temp = file->length + size;
            file->length = temp;
            printf("new_file_size:0x%x\n", temp);
            write_addr = (pos == 0) ? file->block : (file->block - addr_tab[pos]);
            //写新文件大小
            for(uint32_t i = 0; i < 4; i++) {
                *(sector_buffer + write_addr + 16 + i) = ((temp >> (i * 8)) & 0xFF);
            }
            //回写数据
            write_addr = (pos == 0) ? 0x00 : addr_tab[pos];
            sector_erase(write_addr);
            for(uint32_t i = 0; i < 16; i++) {
                w25q32_write_page((sector_buffer + i * 256), 256, (write_addr + i * 256));
            }

            free(sector_buffer);
        }else {
            //剩余空间不够写追加内容

        }

        temp = size - left_size;

        /*
        temp = size - left_size;
        pos = 0;
        //calc sector amounts
        sectors = temp / 4092;
        if((temp % 4092) != 0) {
            sectors += 1;
        }
        //find available sectors
        addr_tab = (uint32_t *)malloc(sizeof(uint32_t) * sectors);
        //start position is 4
        for(uint32_t i = 4; i < 4096; i++) {
            if(pos >= sectors) break;
            if(sector_isempty(i)) {
                *(addr_tab + pos) = i * 0x1000;
                pos++;
            }
        }
        //check available sectors
        if(pos != sectors) return NO_FILE_SECTOR_SPACE;

        for(uint32_t i = 0; i < sectors; i++) {
            mark_sector_inuse(*(addr_tab + i) / 0x1000);
        }
        */
    }

    return 100;
}

void delete_file(File *file) {
    uint8_t state = 0xFF;
    w25q32_read(&state, 1, (file->block + 23));
    state &= ~(0x1);
    write_fileblock_state(file->block, state);
}

FileList *list_file() {

    FileBlock *fb;
    FileList *index = NULL;
    uint32_t addr_start, addr_end;

    uint8_t *cache = (uint8_t *)malloc(sizeof(uint8_t) * 24);

    for(uint32_t i = 0; i < 4; i++) {
        addr_start = addr_tab[i];
        addr_end = ((i + 1) * 0x1000) - addr_start;
        for(; addr_start < addr_end; addr_start += 24) {
            w25q32_read(cache, 24, addr_start);
            //unsafe: only for little endian platform
            fb = (FileBlock *)cache;
            if((fb->state != 0xFFFFFFFF) && (fb->length != 0xFFFFFFFF)) {
                FileList *item = (FileList *)malloc(sizeof(FileList));
                array_copy(fb->filename, item->File.filename, strsize(fb->filename));
                array_copy(fb->extname, item->File.extname, strsize(fb->extname));
                item->File.block = addr_start;
                item->File.cluster = fb->cluster;
                item->File.length = fb->length;
                item->prev = index;
                index = item;
            }
        }
    }
    free(cache);
    return index;
}

void recycle_filelist(FileList *list) {
    FileList *ptr = NULL;
    while(list) {
        ptr = list->prev;
        printf("free: 0x%x\n", (uint32_t)list);
        free(list);
        list = ptr;
    }
}

void system_gc() {

    FileBlock *fb = NULL;
    uint32_t addr_start, addr_end, temp = 0x00;

    uint8_t *slot_buffer = (uint8_t *)malloc(sizeof(uint8_t) * 24);
    uint8_t *sector_flag= (uint8_t *)malloc(sizeof(uint8_t) * 512);
    uint8_t *sector_buffer = (uint8_t *)malloc(sizeof(uint8_t) * 4096);

    w25q32_read(sector_flag, 512, 0x00);

    for(int32_t i = 3; i > -1; i--) {
        addr_start = addr_tab[i];
        addr_end = 0x1000 * (i + 1) - 24;
        w25q32_read(sector_buffer, (addr_end + 24 - addr_start), addr_start);
        for(; addr_start < addr_end; addr_start += 24) {
            w25q32_read(slot_buffer, 24, addr_start);
            fb = (FileBlock *)slot_buffer;
            if(((fb->state >> 24) & 0x1) == 0) {
                //被标记为删除的有效文件
                //根据链表擦除文件数据
                while(fb->cluster != 0xFFFFFFFF) {
                    w25q32_read((uint8_t *)&temp, 4, fb->cluster + 4092);
                    sector_erase(fb->cluster);
                    mark_bit(sector_flag, (fb->cluster / 0x1000));
                    fb->cluster = temp;
                }
                //清除文件结构信息
                clear_fileblock(sector_buffer, (addr_start - addr_tab[i]));
                //标注该文件区域有空位
                if(check_bit(sector_flag, i) == 0) {
                    mark_bit(sector_flag, i);
                }
            }else if((fb->cluster == 0xFFFFFFFF) || (fb->length == 0xFFFFFFFF)) {
                //只有文件头信息而无文件体的文件
                //清除文件结构信息
                clear_fileblock(sector_buffer, (addr_start - addr_tab[i]));
                //标注该文件区域有空位
                if(check_bit(sector_flag, i) == 0) {
                    mark_bit(sector_flag, i);
                }
            }
        }
        //检查该扇区数据是否被更新
        if(check_bit(sector_flag, i) == 1) {
            //擦除该扇区，回写新数据
            sector_erase(((i == 0) ? 0x00 : addr_tab[i]));
            for(uint32_t j = 0 ; j < 16; j++) {
                if(i == 0) {
                    //0扇区需要提前回写使用情况标记
                    if(j < 2) {
                        w25q32_write_page((sector_flag + j * 256), 256, (j * 256));
                    }else {
                        w25q32_write_page((sector_buffer + (j - 2) * 256), 256, (j * 256));
                    }
                }else {
                    w25q32_write_page((sector_buffer + j * 256), 256, (addr_tab[i] + j * 256));
                }
            }
        }
    }
    free(sector_buffer);
    free(sector_flag);
    free(slot_buffer);
}