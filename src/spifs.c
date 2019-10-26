#include "spifs.h"

/*
簇大小 == 扇区大小 == 4KB
sector 0 ~ sector 3 扇区占用索引表,文件索引表

sector0: 0x000 ~ 0x1FF 扇区占用索引表,共512字节
可表示4096个簇占用情况,即最大支持flash容量为16MB

sector0: 0x200 ~ sector3 : 0x3FFF 文件索引表
每个文件索引占24字节,共可支持659个文件

文件簇 = 4KB, 其中数据区4092字节,最后4字节为下一簇物理地址,FFFFFFFF表示文件结束
*/

//FB文件块位于0~3扇区首地址
const uint32_t addr_tab[4] = {0x0200, 0x1000, 0x2000, 0x3000};

/*
创建文件状态字
@param *fstate 状态字段指针
@param year 文件创建年份 以2000年为起点，记录经过年数
@param month 文件创建月份
@param day 文件创建的day of month
*/
void make_fstate(FileState *fstate, uint8_t year, uint8_t month, uint8_t day) {
    fstate->day = day;
    fstate->month = month;
    fstate->year = year;
    fstate->state = 0xFF;
}

/*
创建文件块
@param *file 文件指针
@param filename 文件名称 最大8字符
@param extname 文件拓展名 最大4字符
*/
void make_file(File *file, char *filename, char *extname) {
    file->block = 0x00;
    file->cluster = 0xFFFFFFFF;
    file->length = 0xFFFFFFFF;
    strcopy(filename, file->filename, strsize((uint8_t *)filename));
    strcopy(extname, file->extname, strsize((uint8_t *)extname));
}

/*
创建文件
执行操作: 写文件块记录扇区，可能的full gc
@param *file 文件指针
@param fstate 文件状态字
*/
Result create_file(File *file, FileState fstate) {

    FileBlock *fb = NULL;
    uint8_t has_gc = 0;

    uint32_t idx = 0, addr_start, addr_end, temp = 0x00, count = 0x00;
    uint8_t *slot_buffer = (uint8_t *)malloc(sizeof(uint8_t) * 24);

    //check sector usage table
    FIND_SECTOR_CREATE:
    for(idx = 0; idx < 4; idx++) {
        if(sector_isempty(idx)) {
            break;
        }
    }

    if(idx >= 4) {
        if(has_gc == 0) {
            system_gc();
            has_gc = 1;
            goto FIND_SECTOR_CREATE;
        }
        return NO_FILE_BLOCK_SPACE;
    }

    addr_start = addr_tab[idx];
    addr_end = 0x1000 * (idx + 1) - 24;

    for(; addr_start < addr_end; addr_start += 24) {
        if(count > 1) break;
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

/*
写文件
覆盖写入模式(WRITE):查找空扇区写入数据,并将扇区链表首地址写入文件块记录
追加写入模式(APPEND):当前文件最后扇区剩余空间足够写入数据:直接在文件尾部添加数据
                     当前文件最后扇区不足以写入数据: 先在当前文件尾部写数据，
                     余下部分查找空闲扇区写入，并更新文件扇区链表
对于追加写入模式(APPEND)额外提供append_file()方法,优化对flash擦写次数
//TODO
bug fix
对已有数据的文件实行WRITE，会导致首扇区链表地址更新失败
原数据扇区失去索引而不能被回收

@param *file 文件指针
@param *buffer 写入数据缓冲区
@param size 写入字节数
@param file method 写入方式(覆盖/追加)
*/
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

        Result res = append_file_impl(file, buffer, size, 1);
        return res;
    }

    return UNKNOWN_WRITE_METHOD;
}

/*
追加写文件
此方法适用于频繁调用
追加完毕需调用append_finish()更新文件块记录信息
@param *file 文件指针
@param *buffer 写入数据缓冲区
@param size 写入字节数
*/
Result append_file(File *file, uint8_t *buffer, uint32_t size) {
    Result res = append_file_impl(file, buffer, size, 0);
    return res;
}

/*
追加写完成
更新文件块记录信息
@param *file 文件指针
*/
Result append_finish(File *file) {
    update_fileblock_length(file, file->length);
    return APPEND_FILE_FINISH;
}

/*
删除文件
考虑到flash寿命,此操作不会立即擦除扇区
而将文件状态字标注为被删除,仅在垃圾回收时才会擦除扇区数据
@param *file 文件指针
*/
void delete_file(File *file) {
    uint8_t state = 0xFF;
    w25q32_read(&state, 1, (file->block + 23));
    state &= ~(0x1);
    write_fileblock_state(file->block, state);
}

/*
返回文件列表
以链表形式存储
使用完毕务必调用recycle_filelist()释放文件
*/
FileList *list_file() {
    FileBlock *fb;
    FileList *index = NULL;
    uint32_t addr_start, addr_end;
    uint8_t *cache = (uint8_t *)malloc(sizeof(uint8_t) * 24);

    for(uint32_t i = 0; i < 4; i++) {
        addr_start = addr_tab[i];
        addr_end = ((i + 1) * 0x1000) - addr_start - 24;
        for(; addr_start < addr_end; addr_start += 24) {
            w25q32_read(cache, 24, addr_start);
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

/*
释放文件列表内存
@param *list 文件列表指针
*/
void recycle_filelist(FileList *list) {
    FileList *ptr = NULL;
    while(list) {
        ptr = list->prev;
        printf("free: 0x%x\n", (uint32_t)list);
        free(list);
        list = ptr;
    }
}

Result append_file_impl(File *file, uint8_t *buffer, uint32_t size, uint8_t update) {

    if(file->cluster == 0xFFFFFFFF) return FILE_CAN_NOT_APPEND;
    uint32_t pos = 0, sectors, *sector_list;
    //read old file
    uint32_t next_addr = file->cluster, temp = 0x00;

    //计算文件结束位置
    pos = file->length % 4092;
    pos = (pos == 0) ? 4092 : pos;
    //文件长度超过一扇区，遍历找到最后一个扇区首地址
    if(file->length > 4092) {
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
    uint32_t write_size = 0x00, addr_pos = 0x00;

    if(left_size >= size) {
        //剩余空间足够写追加内容
        temp = size;
        pos = 0;
        while(temp) {
            write_size = (temp >= 256) ? 256 : (temp % 256);
            w25q32_write_page((buffer + pos), write_size, (write_addr + addr_pos));
            pos += write_size;
            temp -= write_size;
            addr_pos += write_size;
        }

        if(update) {
            update_fileblock_length(file, (file->length + size));
        }else {
            file->length += size;
        }
        return APPEND_FILE_SUCCESS;

    }else {
        //剩余空间不够写追加内容
        temp = size - left_size;

        uint8_t has_gc = 0;
        sectors = temp / 4092;
        if((temp % 4092) != 0) {
            sectors += 1;
        }

        sectors = (left_size == 0) ? sectors : (sectors + 1);
        sector_list = (uint32_t *)malloc(sizeof(uint32_t) * sectors);
        *(sector_list + 0) = (left_size == 0) ? 0 : write_addr;

        FIND_SECTOR_APPEND:
        pos = (left_size == 0) ? 0 : 1;
        for(uint32_t i = 4; i < 4096; i++) {
            if(pos >= sectors) break;
            if(sector_isempty(i)) {
                *(sector_list + pos) = i * 0x1000;
                pos++;
            }
        }

        if(pos != sectors) {
            if(has_gc == 0) {
                system_gc();
                has_gc = 1;
                goto FIND_SECTOR_APPEND;
            }
            free(sector_list);
            return NO_FILE_SECTOR_SPACE;
        }

        pos = (left_size == 0) ? 0 : 1;
        for(uint32_t i = pos; i < sectors; i++) {
            mark_sector_inuse(*(sector_list + i) / 0x1000);
        }

        if(left_size == 0) {
            write_value(write_addr, *(sector_list + 0), 4);
        }
        //sector loop
        pos = 0;
        temp = size;
        for(uint32_t i = 0; i < sectors; i++) {
            write_addr = *(sector_list + i);
            write_size = 0;
            addr_pos = 0;
            //page loop
            while(temp) {
                if(addr_pos >= 4092) {
                    write_value((write_addr + addr_pos), *(sector_list + i + 1), 4);
                    break;
                }
                write_size = (temp >= 256) ? 256 : (temp % 256);
                if((addr_pos + write_size) > 4092) {
                    write_size = 4092 - addr_pos;
                }
                w25q32_write_page((buffer + pos), write_size, (write_addr + addr_pos));
                pos += write_size;
                temp -= write_size;
                addr_pos += write_size;
            }
        }
        free(sector_list);

        if(update) {
            update_fileblock_length(file, (file->length + size));
        }else {
            file->length += size;
        }

        return APPEND_FILE_SUCCESS;
    }
}

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

void update_fileblock_length(File *file, uint32_t new_size) {
    uint32_t fb_pos = 0x00, write_addr;
    fb_pos = file->block / 0x1000;
    uint8_t *sector_buffer = (uint8_t *)malloc(sizeof(uint8_t) * 4096);
    w25q32_read(sector_buffer, 4096, (fb_pos * 0x1000));
    //计算新的文件大小
    file->length = new_size;
    write_addr = (fb_pos == 0) ? file->block : (file->block - addr_tab[fb_pos]);
    //写新文件大小
    for(uint32_t i = 0; i < 4; i++) {
        *(sector_buffer + write_addr + 16 + i) = ((new_size >> (i * 8)) & 0xFF);
    }
    //擦除原扇区
    write_addr = (fb_pos * 0x1000);
    sector_erase(write_addr);
    //回写数据
    for(uint32_t i = 0; i < 16; i++) {
        w25q32_write_page((sector_buffer + i * 256), 256, (write_addr + i * 256));
    }
    free(sector_buffer);
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
            sector_erase(i * 0x1000);
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
