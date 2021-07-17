#ifndef __FS_FS_H
#define __FS_FS_H
#include "stdint.h"

#define MAX_FILES_PER_PART 4096     // 每个分区所支持最大创建的文件数
#define BITS_PER_SECTOR 4096        // 每扇区的位数
#define SECTOR_SIZE 512             // 扇区字节大小
#define BLOCK_SIZE SECTOR_SIZE      // 块字节大小

#define MAX_PATH_LEN 512            // 路径最大长度

// 文件类型
enum file_types
{
    FT_UNKNOWN,                     // 不支持的文件类型
    FT_REGULAR,                     // 普通文件
    FT_DIRECTORY                    // 目录
};

// 打开文件的选项
enum oflags
{
    O_RDONLY,                       // 只读
    O_WRONLY,                       // 只写
    O_RDWR,                         // 读写
    O_CREAT = 4                     // 创建
};

// 文件读写位置偏移量
enum whence
{
    SEEK_SET = 1,     // offset的参照物是文件开始处，也就是将读写位置指针设置为距文件开头偏移offset个字节处。
    SEEK_CUR,         // offset 的参照物是当前读写位置，也就是将读写位置指针设置为当前位置＋offset
    SEEK_END          // offset 的参照物是文件尺寸大小，即文件最后一个字节的下一个字节，也就是将读写位置指针设置为文件尺寸＋offset
};

// 用来记录查找文件过程中已找到的上级路径
struct path_search_record
{
    char searched_path[MAX_PATH_LEN];     // 查找过程中的父路径
    struct dir *parent_dir;               // 文件或目录所在的直接父目录
    enum file_types file_type;            // 找到的是普通文件还是目录,找不到将为未知类型(FT_UNKNOWN)
};

// 文件属性结构体
struct stat
{
    uint32_t st_ino;                      // inode编号
    uint32_t st_size;                     // 尺寸
    enum file_types st_filetype;          // 文件类型
};

extern struct partition *cur_part;


// 文件系统初始化
void filesys_init(void);

// 返回路径深度,比如/a/b/c,深度为3
int32_t path_depth_cnt(char *pathname);

// 将最上层路径名称解析出来
char *path_parse(char *pathname, char *name_store);

// 打开或创建文件成功后,返回文件描述符,否则返回-1
int32_t sys_open(const char *pathname, uint8_t flags);

// 关闭文件描述符fd指向的文件,成功返回0,否则返回-1
int32_t sys_close(int32_t fd);

// 将buf中连续count个字节写入文件描述符fd,成功则返回写入的字节数,失败返回-1
int32_t sys_write(int32_t fd, const void *buf, uint32_t count);

// 从文件描述符fd指向的文件中读取count个字节到buf,若成功则返回读出的字节数,到文件尾则返回-1
int32_t sys_read(int32_t fd, void *buf, uint32_t count);

// 重置用于文件读写操作的偏移指针,成功时返回新的偏移量,出错时返回-1
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence);

// 删除文件(非目录),成功返回0,失败返回
int32_t sys_unlink(const char *pathname);

// 创建目录pathname,成功返回0,失败返回-1
int32_t sys_mkdir(const char *pathname);

// 目录打开成功后返回目录指针,失败返回NULL
struct dir *sys_opendir(const char *name);

// 成功关闭目录dir返回0,失败返回-1
int32_t sys_closedir(struct dir *dir);

// 读取目录dir的1个目录项,成功后返回其目录项地址,到目录尾时或出错时返回NULL
struct dir_entry *sys_readdir(struct dir *dir);

// 把目录dir的指针dir_pos置0
void sys_rewinddir(struct dir *dir);

// 删除空目录,成功时返回0,失败时返回-1
int32_t sys_rmdir(const char *pathname);

// 更改当前工作目录为绝对路径path,成功则返回0,失败返回-1
int32_t sys_chdir(const char *path);

// 把当前工作目录绝对路径写入buf
char *sys_getcwd(char *buf, uint32_t size);

// 在buf中填充文件结构相关信息,成功时返回0,失败返回-1
int32_t sys_stat(const char *path, struct stat *buf);

// 向屏幕输出一个字符
void sys_putchar(char char_asci);

// 显示系统支持的内部命令
void sys_help(void);

// 将文件描述符转化为文件表的下标
uint32_t fd_local2global(uint32_t local_fd);

#endif // __FS_FS_H