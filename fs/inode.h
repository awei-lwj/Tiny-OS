#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "stdint.h"
#include "../lib/kernel/list.h"
#include "../device/ide.h"


// inode结构
struct inode
{
    uint32_t i_no;           // inode编号,inode数组中的下表

    // 当此inode是文件时,i_size是指文件大小,若此inode是目录,i_size是指该目录下所有目录项大小之和
    uint32_t i_size;
    uint32_t i_open_cnts;    // 记录此文件被打开的次数
    bool write_deny;         // 写文件不能多个进程一起写,进程写文件前检查此标识

    uint32_t i_sectors[13];  // i_sectors[0-11]是直接块, i_sectors[12]用来存储一级间接块指针

    struct list_elem inode_tag;
};

// 根据i结点号返回相应的i结点
struct inode *inode_open(struct partition *part, uint32_t inode_no);

// 将inode写入到分区part
void inode_sync(struct partition *part, struct inode *inode, void *io_buf);

// 初始化new_inode
void inode_init(uint32_t inode_no, struct inode *new_inode);

// 关闭inode或减少inode的打开数
void inode_close(struct inode *inode);

// 回收inode的数据块和inode本身
void inode_release(struct partition *part, uint32_t inode_no);

// 将硬盘分区part上的inode清空
void inode_delete(struct partition *part, uint32_t inode_no, void *io_buf);

/**
//-----------------------------------------------------------------------------------------------------------------------
文件描述符与inode的关系

                                        +------------------+
文件描述符号的数组                         |        |  fd_pos |
    |            PCB             +----->| 文件结构 |fd_inode |----+        
    |    +------------------+    |      |        | fd_flag |     |
    |    |                  |    |      +------------------+     |
    |    |                  |    |                               |
    |    |------------------|    |                               |
    |    |                  |    |                               |   
    |--->|   文件结构的下标   |----+                               |
    |    |                  |                                    |
    |    |------------------|                                    |   +------------------+ 
    |    |                  |                                    |   |                  |
    |    |                  |                                    |   |                  |                 +------------------+
    |    |                  |                                    |-->|      inode       |---------------->|       磁盘        |    
    |    |     .......      |                                        |                  |                 +------------------+
    |    |                  |                                        |                  |
    |    |                  |                                        +------------------+
    |    |------------------|
    |    |                  |   
    |--->|   文件结构的下标   |
         |                  |
         |------------------|
         |                  |
         |                  |
         |                  |
         +------------------+

||-----------------------------------    内存    ----------------------------------------|---------------  外存   -------------|



//-----------------------------------------------------------------------------------------------------------------------
**/

#endif // __FS_INODE_H
