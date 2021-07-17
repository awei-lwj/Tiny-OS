#ifndef __FS_DIR_H
#define __FS_DIR_H
#include "stdint.h"
#include "inode.h"
#include "fs.h"
#include "../device/ide.h"
#include "../kernel/global.h"


#define MAX_FILE_NAME_LEN 16       // 最大文件名长度,为dir_entry准备的

// 目录结构,只能用于目录相关的操作，在内存中创建的结构，用过之后释放，不会保留在硬盘中
struct dir
{
    struct inode *inode;
    uint32_t dir_pos;              // 记录在目录内的偏移
    uint8_t dir_buf[512];          // 目录的数据缓存
};

// 目录项结构
struct dir_entry
{
    char filename[MAX_FILE_NAME_LEN];  // 普通文件或目录名称
    uint32_t i_no;                     // 普通文件或目录对应的inode编号
    enum file_types f_type;            // 文件类型
};

extern struct dir root_dir;            // 根目录


// 打开分区part根目录
void open_root_dir(struct partition *part);

// 在分区part上打开i结点为inode_no的目录并返回目录指针
struct dir *dir_open(struct partition *part, uint32_t inode_no);

// 关闭目录
void dir_close(struct dir *dir);

// 在part分区内的pdir目录内寻找名为name的文件或目录,089// 找到后返回true并将其目录项存入dir_e,否则返回false
bool search_dir_entry(struct partition *part, struct dir *pdir, const char *name, struct dir_entry *dir_e);

// 在内存中初始化目录项p_de
void create_dir_entry(char *filename, uint32_t inode_no, uint8_t file_type, struct dir_entry *p_de);

// 将目录项p_de写入父目录parent_dir中,io_buf由主调函数提
bool sync_dir_entry(struct dir *parent_dir, struct dir_entry *p_de, void *io_buf);

// 把分区part目录pdir中编号为inode_no的目录项删除
bool delete_dir_entry(struct partition *part, struct dir *pdir, uint32_t inode_no, void *io_buf);

// 读取目录,成功返回1个目录项,失败返回NULL
struct dir_entry *dir_read(struct dir *dir);

// 判断目录是否为空
bool dir_is_empty(struct dir *dir);

// 在父目录parent_dir中删除child_dir
int32_t dir_remove(struct dir *parent_dir, struct dir *child_dir);



#endif // __FS_DIR_H