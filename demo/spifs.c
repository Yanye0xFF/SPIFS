#include "spifs.h"

/**
 * 文件簇大小 = 扇区大小 = 4KB
 * 文件簇: 扇区标记字2字节, 数据区4090字节, 最后4字节为下一簇物理地址, FFFFFFFF表示文件结束
 * */

void update_fileblock_length(File *file);

/**
 * 创建文件状态字
 * @param *fstate 状态字段指针
 * @param year 文件创建年份(2000~2255),以2000年为起点
 * @param month 文件创建月份(1~12)
 * @param day 文件创建的日期(1~N),N: day of month
 * */
void make_fstate(FileState *fstate, uint32_t year, uint8_t month, uint8_t day) {
    fstate->day = day;
    fstate->month = month;
    fstate->year = (year - 2000);
    fstate->state = 0xFF;
}

/**
 * 创建文件块
 * @param *file 文件指针
 * @param filename 文件名称 最大8字符
 * @param extname 文件拓展名 最大4字符
 * */
void make_file(File *file, char *filename, char *extname) {
    file->block = 0xFFFFFFFF;
    file->cluster = 0xFFFFFFFF;
    file->length = 0xFFFFFFFF;
    copy_filename(filename, file->filename, strlen(filename), sizeof(file->filename));
    copy_filename(extname, file->extname, strlen(extname), sizeof(file->extname));
}

/**
 * 创建文件
 * 写文件块记录扇区,空间不足时执行垃圾回收
 * @param *file 文件指针
 * @param fstate 文件状态字
 * */
Result create_file(File *file, FileState fstate) {
    FileBlock *fb = NULL;
    uint32_t fb_index, addr_start, addr_end;

    uint8_t find_empty_sector = 0, is_empty_fb, gc_flag = 0;
    uint8_t *slot_buffer = (uint8_t *)malloc(sizeof(uint8_t) * FILEBLOCK_SIZE);

    FIND_FB_SPACE:
    for(fb_index = FB_SECTOR_INIT; (fb_index < FB_SECTOR_END) && (find_empty_sector == 0); fb_index++) {
        addr_start = fb_index * SECTOR_SIZE;
        addr_end = addr_start + SECTOR_SIZE;
        while((addr_end - addr_start) >= FILEBLOCK_SIZE) {
            is_empty_fb = 1;
            disk_read(addr_start, slot_buffer, FILEBLOCK_SIZE);
            // check filename and extname
            for(uint32_t i = 0; i < FILENAME_FULLSIZE; i++) {
                if(*(slot_buffer + i) != 0xFF) {
                    is_empty_fb = 0;
                    addr_start += FILEBLOCK_SIZE;
                    break;
                }
            }
            if(is_empty_fb) {
                find_empty_sector = 1;
                break;
            }
        }
    }
    // run gc
    if(find_empty_sector == 0) {
        if(gc_flag == 1) {
            return NO_FILEBLOCK_SPACE;
        }
        gc_flag = 1;
        spifs_gc();
        // retry to find space for fileblock
        goto FIND_FB_SPACE;
    }
    // clear fileblock buffer
    array_fill(slot_buffer, 0xFF, FILEBLOCK_SIZE);
    fb = (FileBlock *)slot_buffer;

    array_copy(file->filename, fb->filename, 8);
    array_copy(file->extname, fb->extname, 4);
    fb->state = *(uint32_t *)&fstate;

    write_fileblock(addr_start, fb);

    file->block = addr_start;
    file->cluster = fb->cluster;
    file->length = fb->length;

    free(slot_buffer);
    return CREATE_FILEBLOCK_SUCCESS;
}

/**
 * 覆盖写文件
 * 无数据文件:查找空扇区写入数据,更新文件块记录
 * 存在数据文件:擦除旧文件,查找空扇区写入数据,更新文件块记录
 * @param *file 文件指针
 * @param *buffer 写入数据缓冲区
 * @param size 写入字节数
 * */
Result write_file(File *file, uint8_t *buffer, uint32_t size) {
    uint32_t addr_cluster;
    uint8_t sector_inuse, *sector_buffer;
    uint32_t sector_index, sectors, count, *sector_list;

    if(file->block == 0xFFFFFFFF) return FILE_UNALLOCATED;
    // 文件存在数据则擦除数据扇区与文件索引表对应项
    if(file->cluster != 0xFFFFFFFF || file->length != 0xFFFFFFFF) {
        // 根据链表擦除文件占用扇区
        while(file->cluster != 0xFFFFFFFF) {
            disk_read(file->cluster + DATA_AREA_SIZE + 2, (uint8_t *)&addr_cluster, 4);
            sector_erase(file->cluster);
            file->cluster = addr_cluster;
        }
        // 更新文件索引表项,擦除扇区首地址与文件大小
        sector_index = (file->block / SECTOR_SIZE) * SECTOR_SIZE;
        addr_cluster = file->block % SECTOR_SIZE;
        sector_buffer = (uint8_t *)malloc(sizeof(uint8_t) * SECTOR_SIZE);
        disk_read(sector_index, sector_buffer, SECTOR_SIZE);
        for(uint32_t i = 0; i < 8; i++) {
            *(sector_buffer + addr_cluster + FILENAME_FULLSIZE + i) = 0xFF;
        }
        sector_erase(sector_index);
        for(uint32_t i = 0 ; i < 16; i++) {
            disk_write((sector_index * SECTOR_SIZE + i * PAGE_SIZE), (sector_buffer + i * PAGE_SIZE), PAGE_SIZE);
        }
        free(sector_buffer);
    }
    // 计算buffer下数据需要占用的扇区数
    sectors = size / DATA_AREA_SIZE;
    if((size % DATA_AREA_SIZE) != 0) {
        sectors += 1;
    }

    count = 0;
    sector_list = (uint32_t *)malloc(sizeof(uint32_t) * sectors);
    for(sector_index = FB_SECTOR_END; sector_index < SECTOR_SUM; sector_index++) {
        disk_read(sector_index * SECTOR_SIZE, &sector_inuse, 1);
        if(sector_inuse == 0xFF) {
            *(sector_list + count) = sector_index * SECTOR_SIZE;
            count++;
        }
        if(count >= sectors) {
            break;
        }
    }

    if(count != sectors) return NO_SECTOR_SPACE;

    uint32_t write_size, write_addr, addr_position;
    // 更新文件索引信息
    write_fileblock_cluster(file->block, *(sector_list + 0));
    write_fileblock_length(file->block, size);
    file->cluster = *(sector_list + 0);
    file->length = size;
    count = 0;
    // sector loop
    for(uint32_t i = 0; i < sectors; i++) {
        // 写占用标记
        write_value(*(sector_list + i), 0xFF00, 2);
        // 扇区地址偏移2字节
        write_addr = *(sector_list + i) + 2;
        write_size = 0;
        addr_position = 0;
        //page loop
        while(size) {
            if(addr_position >= DATA_AREA_SIZE) {
                // 写下一扇区地址,跳出循环更换扇区
                write_value((write_addr + DATA_AREA_SIZE), *(sector_list + i + 1), 4);
                break;
            }
            write_size = (size >= PAGE_SIZE) ? PAGE_SIZE : (size % PAGE_SIZE);
            if((addr_position + write_size) > DATA_AREA_SIZE) {
                write_size = DATA_AREA_SIZE - addr_position;
            }

            disk_write((write_addr + addr_position), (buffer + count), write_size);
            count += write_size;
            size -= write_size;
            addr_position += write_size;
        }
    }
    free(sector_list);
    return WRITE_FILE_SUCCESS;
}

/**
 * 追加写文件
 * 在文件尾部添加数据
 * 适用频繁调用场合
 * 追加完毕需调用append_finish更新文件块记录信息
 * @param *file 文件指针
 * @param *buffer 写入数据缓冲区
 * @param size 写入字节数
 * */
Result append_file(File *file, uint8_t *buffer, uint32_t size) {

    if(file->cluster == 0xFFFFFFFF) return FILE_CANNOT_APPEND;

    uint8_t gc_flag = 0, zero_flag = 0;
    uint32_t cursor, temp = 0;
    uint32_t next_addr = file->cluster;

    uint32_t sectors, *sector_list;

    uint32_t left_size, write_addr;
    uint32_t write_size = 0, addr_position = 0;

    //计算文件结束位置(相对于扇区起始位置偏移量)
    cursor = (file->length % DATA_AREA_SIZE) + SECTOR_STATE_SIZE;
    // 遍历找到最后一个扇区首地址
    if(file->length >= DATA_AREA_SIZE) {
        sectors = file->length / DATA_AREA_SIZE;
        for(uint32_t i = 0; i < sectors; i++) {
            disk_read((next_addr + (DATA_AREA_SIZE + SECTOR_STATE_SIZE)), (uint8_t *)&temp, 4);
            if(temp == 0xFFFFFFFF) {
                zero_flag = 1;
                break;
            }
            next_addr = temp;
        }
    }

    if(zero_flag) {
        left_size = 0;
        write_addr = next_addr + SECTOR_STATE_SIZE + DATA_AREA_SIZE;
    }else {
        // 计算结束扇区空余空间
        left_size = (next_addr + SECTOR_STATE_SIZE + DATA_AREA_SIZE) - (next_addr + cursor);
        // 追加模式写新内容起始地址
        write_addr = next_addr + cursor;
    }

    if(left_size >= size) {
        //结束扇区剩余空间足够写追加内容
        cursor = 0;
        file->length += size;
        while(size) {
            write_size = (size >= PAGE_SIZE) ? PAGE_SIZE : (size % PAGE_SIZE);
            disk_write((write_addr + addr_position), (buffer + cursor), write_size);
            cursor += write_size;
            addr_position += write_size;
            size -= write_size;
        }
        return APPEND_FILE_SUCCESS;
    }
    // 结束扇区剩余空间不够写追加内容
    temp = size - left_size;
    // 计算余下文件内容需要的扇区数量(当前最后扇区也计入)
    sectors = (temp / DATA_AREA_SIZE) + 1;
    sectors = (temp % DATA_AREA_SIZE) ? (sectors + 1) : sectors;

    sector_list = (uint32_t *)malloc(sizeof(uint32_t) * sectors);
    *(sector_list + 0) = next_addr;

    // 查找空扇区
    FIND_SECTOR_APPEND:
    cursor = 1;
    for(uint32_t sector_index = FB_SECTOR_END; sector_index < SECTOR_SUM; sector_index++) {
        disk_read(sector_index * SECTOR_SIZE, (uint8_t *)&temp, 1);
        if((temp & 0xFF) == 0xFF) {
            *(sector_list + cursor) = sector_index * SECTOR_SIZE;
            cursor++;
        }
        if(cursor >= sectors) {
            break;
        }
    }

    // 验证空闲扇区数量是否足以写入文件
    if(cursor != sectors) {
        if(gc_flag == 1) {
            free(sector_list);
            return NO_SECTOR_SPACE;
        }
        gc_flag = 1;
        spifs_gc();
        goto FIND_SECTOR_APPEND;
    }

    cursor = 0;
    file->length += size;
    // sector loop
    for(uint32_t i = 0; i < sectors; i++) {
        if(i > 0) {
            left_size = DATA_AREA_SIZE;
            write_value(*(sector_list + i), 0xFF00, SECTOR_STATE_SIZE);
            write_addr = *(sector_list + i) + SECTOR_STATE_SIZE;
        }
        write_size = 0;
        addr_position = 0;
        //page loop
        while(size) {
            if(addr_position >= left_size) {
                // 写下一扇区地址,跳出循环更换扇区
                write_value((write_addr + addr_position), *(sector_list + i + 1), 4);
                break;
            }
            write_size = (size >= PAGE_SIZE) ? PAGE_SIZE : (size % PAGE_SIZE);
            if((addr_position + write_size) > left_size) {
                write_size = left_size - addr_position;
            }
            disk_write((write_addr + addr_position), (buffer + cursor), write_size);
            cursor += write_size;
            size -= write_size;
            addr_position += write_size;
        }
    }

    free(sector_list);
    return APPEND_FILE_SUCCESS;
}

/**
 * 追加写完成
 * 更新文件块记录信息
 * @param *file 文件指针
 * @return APPEND_FILE_FINISH 追加写完成,更新文件索引的length字段
 * */
Result append_finish(File *file) {
    update_fileblock_length(file);
    return APPEND_FILE_FINISH;
}

/**
 * 根据文件名+拓展名打开文件
 * @param file 文件指针
 * @param filename 文件名
 * @param extname 拓展名
 * @return 0:未找到该文件, 1:成功获取文件
 * */
uint8_t open_file(File *file, char *filename, char *extname) {
    FileBlock *fb;
    uint32_t addr_start, addr_end;
    uint8_t *slot_buffer = (uint8_t *)malloc(sizeof(uint8_t) * FILEBLOCK_SIZE);

    for(uint32_t i = FB_SECTOR_INIT; i < FB_SECTOR_END; ++i) {
        addr_start = i * SECTOR_SIZE;
        addr_end = addr_start + SECTOR_SIZE;
        while((addr_end - addr_start) >= FILEBLOCK_SIZE) {
            disk_read(addr_start, slot_buffer, FILEBLOCK_SIZE);
            fb = (FileBlock *)slot_buffer;
            if(comp_filename(fb->extname, extname, strlen(extname)) && comp_filename(fb->filename, filename, strlen(filename))) {
                file->block = addr_start;
                file->cluster = fb->cluster;
                file->length = fb->length;
                copy_filename(filename, file->filename, strlen(filename), 8);
                copy_filename(extname, file->extname, strlen(extname), 4);
                free(slot_buffer);
                return 1;
            }
            addr_start += FILEBLOCK_SIZE;
        }
    }
    free(slot_buffer);
    return 0;
}

uint8_t read_state(File *file, FileState *state) {
    FileBlock *fb;
    uint8_t *slot_buffer = (uint8_t *)malloc(sizeof(uint8_t) * FILEBLOCK_SIZE);
    disk_read(file->block, slot_buffer, FILEBLOCK_SIZE);
    fb = (FileBlock *)slot_buffer;
    *state = *(FileState *)&fb->state;
    free(slot_buffer);
    return 1;
}

uint8_t read_file(File *file, uint8_t *buffer, uint32_t offset, uint32_t size) {
    uint32_t cursor = 0, read_size;
    uint32_t addr_start = file->cluster;
    uint32_t addr_cluster = 0, cluster_limit;
    uint32_t sectors = offset / DATA_AREA_SIZE;
    // 边界检查
    if(offset >= file->length || (file->length - offset) < size) {
        return 0;
    }
    for(uint32_t i = 0; i < sectors; i++) {
        disk_read((addr_start + SECTOR_STATE_SIZE + DATA_AREA_SIZE), (uint8_t *)&addr_cluster, 4);
        addr_start = addr_cluster;
    }
    // 扇区读写地址范围
    cluster_limit = addr_start + SECTOR_STATE_SIZE + DATA_AREA_SIZE;
    addr_start = addr_start + SECTOR_STATE_SIZE + (offset - sectors * DATA_AREA_SIZE);

    while(size) {
        // 计算分块读取块大小
        read_size = (size > 256) ? 256 : size;
        read_size = ((addr_start + read_size) > cluster_limit) ? (cluster_limit - addr_start) : read_size;
        // 切换下一扇区
        if(read_size <= 0) {
            disk_read(cluster_limit, (uint8_t *)&addr_cluster, 4);
            cluster_limit = addr_cluster + SECTOR_STATE_SIZE + DATA_AREA_SIZE;
            addr_start = addr_cluster + SECTOR_STATE_SIZE;
            continue;
        }
        // 读取数据
        disk_read(addr_start, (buffer + cursor), read_size);
        // 更新地址偏移
        addr_start += read_size;
        cursor += read_size;
        size -= read_size;
    }
    return 1;
}

/**
 * 删除文件, 此操作不会立即擦除扇区
 * 而将文件状态字标注为被删除,仅在垃圾回收时才会擦除扇区数据
 * @param *file 文件指针
 * */
void delete_file(File *file) {
    uint8_t state = 0xFF;
    disk_read((file->block + 23), &state, 1);
    state &= ~0x1;
    write_fileblock_state(file->block, state);
}

/**
 * 返回文件列表
 * 以链表形式存储
 * 使用完毕务必调用recycle_filelist()释放文件
 * */
FileList *list_file() {
    FileBlock *fb;
    FileList *index = NULL;
    uint32_t addr_start, addr_end;
    uint8_t *cache = (uint8_t *)malloc(sizeof(uint8_t) * FILEBLOCK_SIZE);

    for(uint32_t i = FB_SECTOR_INIT; i < FB_SECTOR_END; i++) {
        addr_start = i * SECTOR_SIZE;
        addr_end = addr_start + SECTOR_SIZE;

        while(addr_end - addr_start >= FILEBLOCK_SIZE) {
            disk_read(addr_start, cache, FILEBLOCK_SIZE);
            fb = (FileBlock *)cache;
            if((fb->state != 0xFFFFFFFF) && (fb->length != 0xFFFFFFFF)) {
                FileList *item = (FileList *)malloc(sizeof(FileList));
                array_copy(fb->filename, item->File.filename, 8);
                array_copy(fb->extname, item->File.extname, 4);
                item->File.block = addr_start;
                item->File.cluster = fb->cluster;
                item->File.length = fb->length;
                item->prev = index;
                index = item;
            }
            addr_start += FILEBLOCK_SIZE;
        }
    }
    free(cache);
    return index;
}

/**
 * 释放文件列表内存
 * @param *list 文件列表指针
 * */
void recycle_filelist(FileList *list) {
    FileList *ptr = NULL;
    while(list) {
        ptr = list->prev;
        free(list);
        list = ptr;
    }
}

void update_fileblock_length(File *file) {
    uint32_t fb_sector, write_addr;
    uint8_t *sector_buffer = (uint8_t *)malloc(sizeof(uint8_t) * SECTOR_SIZE);
    // 文件块所在扇区首地址
    fb_sector = (file->block / SECTOR_SIZE) * SECTOR_SIZE;
    disk_read(fb_sector, sector_buffer, SECTOR_SIZE);
    write_addr = (file->block - fb_sector);
    //写新文件大小
    for(uint32_t i = 0; i < 4; i++) {
        *(sector_buffer + write_addr + 16 + i) = ((file->length >> (i << 3)) & 0xFF);
    }
    //擦除原扇区
    sector_erase(fb_sector);
    //回写数据
    for(uint32_t i = 0; i < 16; i++) {
        disk_write((fb_sector + i * 256), (sector_buffer + i * 256), 256);
    }
    free(sector_buffer);
}

/**
 * spifs垃圾回收
 * 应用层的删除文件操作并不会从闪存中擦除文件数据
 * 而是标记其文件块的状态属性为可删除文件
 * 当空间不足时才进行全盘扫描, 删除标记的文件数据
 * */
void spifs_gc() {

    FileBlock *fb = NULL;

    uint8_t rewrite = 0;
    uint32_t offset = 0, fb_index, addr_cluster;

    uint8_t *slot_buffer = (uint8_t *)malloc(sizeof(uint8_t) * FILEBLOCK_SIZE);
    uint8_t *sector_buffer = (uint8_t *)malloc(sizeof(uint8_t) * SECTOR_SIZE);

    for(fb_index = FB_SECTOR_INIT; fb_index < FB_SECTOR_END; fb_index++) {

        disk_read((fb_index * SECTOR_SIZE), sector_buffer, SECTOR_SIZE);

        while((SECTOR_SIZE - offset) >= FILEBLOCK_SIZE) {

            array_copy((sector_buffer + offset), slot_buffer, FILEBLOCK_SIZE);
            fb = (FileBlock *)slot_buffer;

            // 文件被标识为删除
            if(((fb->state >> 24) & 0x1) == 0) {
                // 根据链表擦除文件占用扇区
                while(fb->cluster != 0xFFFFFFFF) {
                    disk_read((fb->cluster + SECTOR_STATE_SIZE + DATA_AREA_SIZE), (uint8_t *)&addr_cluster, 4);
                    sector_erase(fb->cluster);
                    fb->cluster = addr_cluster;
                }
                // 清除文件索引信息
                clear_fileblock(sector_buffer, offset);
                rewrite = 1;
            }
            // 创建文件但未填充数据
            if(fb->cluster == 0xFFFFFFFF) {
                // 清除文件索引信息
                clear_fileblock(sector_buffer, offset);
                rewrite = 1;
            }
            offset += FILEBLOCK_SIZE;
        }
        // 擦除文件索引扇区，回写新文件索引表
        if(rewrite == 1) {
            rewrite = 0;
            sector_erase(fb_index * SECTOR_SIZE);
            for(uint32_t i = 0 ; i < 16; i++) {
                disk_write((fb_index * SECTOR_SIZE + i * PAGE_SIZE), (sector_buffer + i * PAGE_SIZE), PAGE_SIZE);
            }
        }
    }
    free(sector_buffer);
    free(slot_buffer);
}
