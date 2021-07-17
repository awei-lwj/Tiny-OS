#ifndef __FS_FILE_H
#define __FS_FILE_H
#include "stdint.h"
#include "dir.h"
#include "../device/ide.h"
#include "../kernel/global.h"


#define MAX_FILE_OPEN 32     // 系统可打开的最大文件数

// 文件结构
struct file
{
    uint32_t fd_pos;         // 记录当前文件操作的偏移地址,以0为起始,最大为文件大小-1
    uint32_t fd_flag;
    struct inode *fd_inode;
};

// 标准输入输出描述符
enum std_fd
{
    stdin_no,                // 0 标准输入
    stdout_no,               // 1 标准输出
    stderr_no                // 2 标准错误
};

// 位图类型
enum bitmap_type
{
    INODE_BITMAP,            // inode位图
    BLOCK_BITMAP             // 块位图
};

extern struct file file_table[MAX_FILE_OPEN];


// 分配一个i结点,返回i结点号
int32_t inode_bitmap_alloc(struct partition *part);

// 分配1个扇区,返回其扇区地址
int32_t block_bitmap_alloc(struct partition *part);

// 创建文件
int32_t file_create(struct dir *parent_dir, char *filename, uint8_t flag);

// 将内存中bitmap第bit_idx位所在的512字节同步到硬盘
void bitmap_sync(struct partition *part, uint32_t bit_idx, uint8_t btmp);

// 从文件表file_table中获取一个空闲位,成功返回下标,失败返回-1
int32_t get_free_slot_in_global(void);

// 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中,
int32_t pcb_fd_install(int32_t global_fd_idx);

// 打开文件
int32_t file_open(uint32_t inode_no, uint8_t flag);

// 关闭文件
int32_t file_close(struct file *file);

// 写文件
int32_t file_write(struct file *file, const void *buf, uint32_t count);

// 读文件
int32_t file_read(struct file *file, void *buf, uint32_t count);

#endif // __FS_FILE_H
