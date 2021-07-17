#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H
#include "stdint.h"
#include "../fs/fs.h"
#include "../thread/thread.h"

// 用来存放系统调用子功能号，以后再增加新的调用后还需要把新的子功能号添加到此结构中
enum SYSCALL_NR
{
    SYS_GETPID,      // 系统调用获取PID
    SYS_WRITE,       // 系统调用进行读写
    SYS_MALLOC,      // 系统调用分配内存
    SYS_FREE,        // 释放内存
    SYS_FORK,        // 创建子进程
    SYS_READ,        // 从文件描述符fd中读取count个字节到buf
    SYS_PUTCHAR,     // 输出字符
    SYS_CLEAR,       // 清空屏幕
    SYS_GETCWD,      // 当前工作目录
    SYS_OPEN,        // 以flag方式打开文件pathname
    SYS_CLOSE,       // 关闭文件fd
    SYS_LSEEK,       // 设置文件偏移量
    SYS_UNLINK,      // 删除文件pathname
    SYS_MKDIR,       // 创建目录pathname
    SYS_OPENDIR,     // 打开目录name
    SYS_CLOSEDIR,    // 关闭目录dir
    SYS_CHDIR,       // 改变工作目录为path
    SYS_RMDIR,       // 删除目录pathname
    SYS_READDIR,     // 读取目录dir
    SYS_REWINDDIR,   // 回归目录指针
    SYS_STAT,        // 获取path属性到buf中
    SYS_PS,          // 显示任务列表
    SYS_EXECV,       // 执行pathname
    SYS_EXIT,        // 以状态status退出
    SYS_WAIT,        // 等待子进程,子进程状态存储到status
    SYS_PIPE,        // 管道
    SYS_FD_REDIRECT, // 文件从定向
    SYS_HELP         // 显示系统支持的命令
};


// 返回当前任务pid
uint32_t getpid(void);

// 写
uint32_t write(int32_t fd, const void *buf, uint32_t count);

// 申请size字节大小的内存,并返回结果
void *malloc(uint32_t size);

// 释放ptr指向的内存
void free(void *ptr);

// 派生子进程,返回子进程pid
int16_t fork(void);

// 从文件描述符fd中读取count个字节到buf
int32_t read(int32_t fd, void *buf, uint32_t count);

// 输出一个字符
void putchar(char char_asci);

// 清空屏幕
void clear(void);

// 获取当前工作目录
char *getcwd(char *buf, uint32_t size);

// 以flag方式打开文件pathname
int32_t open(char *pathname, uint8_t flag);

// 关闭文件fd
int32_t close(int32_t fd);

// 设置文件偏移量
int32_t lseek(int32_t fd, int32_t offset, uint8_t whence);

// 删除文件pathname
int32_t unlink(const char *pathname);

// 打开目录name
struct dir *opendir(const char *name);

// 关闭目录dir
int32_t closedir(struct dir *dir);

// 创建目录pathname
int32_t mkdir(const char *pathname);

// 删除目录pathname
int32_t rmdir(const char *pathname);

// 读取目录dir
struct dir_entry *readdir(struct dir *dir);

// 回归目录指针
void rewinddir(struct dir *dir);

// 获取path属性到buf中
int32_t stat(const char *path, struct stat *buf);

// 显示任务列表
void ps(void);

// 改变工作目录为path
int32_t chdir(const char *path);

// 执行pathname
int execv(const char *pathname, char **argv);

// 以状态status退出
void exit(int32_t status);

// 等待子进程,子进程状态存储到status
pid_t wait(int32_t *status);

// 生成管道,pipefd[0]负责读入管道,pipefd[1]负责写入管道
int32_t pipe(int32_t pipefd[2]);

// 将文件描述符old_local_fd重定向到new_local_fd
void fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd);

// 显示系统支持的命令
void help(void);

#endif // __LIB_USER_SYSCALL_H
