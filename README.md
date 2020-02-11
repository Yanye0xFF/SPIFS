## SPIFS
非常简单的文件系统，核心代码约500行，适用于256字节/页，4096字节/扇区的存储设备；主要应用于spi flash器件，例如w25q32、w25q64、w25q128等。  
实现了基本的文件管理功能例如创建、写入、追加、读取等，对于删除文件采用标记清除的回收方式，删除文件时仅对其进行标记，在后续的文件写入过程中若空间不足，再进行擦除工作。  
本文件系统采用单一文件的布局，并不支持文件夹；使用8+4文件名(类似于FAT的短文件名只是后缀由3字节变为4字节)，文件名8字节与后缀名4字节可连在一起使用。

## 目录说明
src：文件系统实现源码，w25q32.c模拟了一个spi flash器件。  
demo：codeblocks演示项目，在gcc-4.8.2 x64 (posix)下验证通过。
## api说明
使用文件名(filename)，和(extname)拓展名创建文件，此时存储器并未并未写入任何内容，  
只是将filename和extname复制进file。
```c
void make_file(File *file, char *filename, char *extname)
```

创建文件状态，year范围为2000~2255
```c
void make_fstate(FileState *fstate, uint32_t year, uint8_t month, uint8_t day)
```

创建文件，此时文件信息已写入文件块索引扇区
```c
Result create_file(File *file, FileState fstate)
```

写文件，查找空扇区填充数据，完成后更新文件块的大小和首簇地址
```c
Result write_file(File *file, uint8_t *buffer, uint32_t size)
```

追加写文件，查找空扇区填充数据，可多次调用，  
在最后一次调用完成后需要使用append_finish更新文件块的大小信息
```c
Result append_file(File *file, uint8_t *buffer, uint32_t size)
Result append_finish(File *file);
```

使用文件名+拓展名查找/打开文件
```c
uint8_t open_file(File *file, char *filename, char *extname)
```

读取文件状态信息，例如创建时间，文件状态(是否标记为删除)
```c
uint8_t read_state(File *file, FileState *state)
```

读取文件
```c
uint8_t read_file(File *file, uint8_t *buffer, uint32_t offset, uint32_t size)
```

删除文件
```c
void delete_file(File *file)
```

垃圾回收，通常由文件系统自身调用
```c
void spifs_gc();
```

列出存储器上的所有文件信息，返回文件链表，
使用完成务必调用recycle_filelist释放文件链表
```c
FileList *list_file()
void recycle_filelist(FileList *list)
```

## 文件系统结构图示

扇区大小与文件簇大小相同  
![image](https://raw.githubusercontent.com/Yanye0xFF/PictureBed/master/images/spifs/sector_size.png)  

存储器数据布局，文件索引块占用0~3扇区  
![image](https://raw.githubusercontent.com/Yanye0xFF/PictureBed/master/images/spifs/total_view.png)  

文件索引块结构  
![image](https://raw.githubusercontent.com/Yanye0xFF/PictureBed/master/images/spifs/index_struct.png)  

文件索引块->状态字结构  
![image](https://raw.githubusercontent.com/Yanye0xFF/PictureBed/master/images/spifs/state_struct.png)  

文件索引块->状态字->标记位  
![image](https://raw.githubusercontent.com/Yanye0xFF/PictureBed/master/images/spifs/flag_struct.png)  

数据域结构  
![image](https://raw.githubusercontent.com/Yanye0xFF/PictureBed/master/images/spifs/data_area.png)  
