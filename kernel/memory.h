#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "../lib/kernel/bitmap.h"
#include "../lib/kernel/list.h"

#define PG_P_1  1            // 页表项或页目录项存在属性位,P位的值为1，表示此页内存己存在
#define PG_P_0  0            // 页表项或页目录项存在属性位,P位的值为0，表示此页内存不W存在
#define PG_RW_R 0            // R/W 属性位值, 读/执行，RW位的值为0，即RW=0，表示此页内存允许读、执行。
#define PG_RW_W 2            // R/W 属性位值, 读/写/执行，RW位的值为W，即RW=1，表此页内存允许读、写、执行
#define PG_US_S 0            // U/S 属性位值, 系统级，US=O，表示只允许特权级别为0、1、2的程序访问此页内存，特权级3程序不被允许。
#define PG_US_U 4            // U/S 属性位值, 用户，表示允许所有特权级别程序访问此页内存。

// 16 32 64 128 256 512 1024
#define DESC_CNT 7           // 内存块描述符个数

// 内存池标记,用于判断用哪个内存池
enum pool_flags
{
    PF_KERNEL = 1,           // 内核内存池
    PF_USER = 2              // 用户内存池
};

// 虚拟地址池，用于虚拟地址管理
struct virtual_addr
{
    // 虚拟地址用到的位图结构，在bitmap.h上定义了，保证了同一个进程内的虚拟地址必然是唯一的
    struct bitmap vaddr_bitmap; 

    uint32_t vaddr_start;       // 虚拟地址起始地址
};

// 内存块
struct mem_block
{
    struct list_elem free_elem;
};

// 内存块描述符
struct mem_block_desc
{
    uint32_t block_size;       // 内存块大小
    uint32_t blocks_per_arena; // 本arena中可容纳此mem_block的数量.
    struct list free_list;     // 目前可用的mem_block链表
};


extern struct pool kernel_pool, user_pool; // 用来生成内核地址和用户地址


// 内存管理部分初始化入口
void mem_init(void);  

// 从内核物理内存池中申请pg_cnt页内存,成功则返回其虚拟地址,失败则返回NULL
void *get_kernel_pages(uint32_t pg_cnt);  

// 分配pg_cnt个页空间,成功则返回起始虚拟地址,失败时返回NULL
void *malloc_page(enum pool_flags pf, uint32_t pg_cnt); 

// 得到虚拟地址vaddr对应的pte指针
uint32_t *pte_ptr(uint32_t vaddr); 

// 得到虚拟地址vaddr对应的pde的指针
uint32_t *pde_ptr(uint32_t vaddr); 

// 得到虚拟地址映射到的物理地址
uint32_t addr_v2p(uint32_t vaddr); 

// 将地址vaddr与pf池中的物理地址关联,仅支持一页空间分配
void *get_a_page(enum pool_flags pf, uint32_t vaddr);

// 在用户空间中申请4k内存，并返回虚地址
void *get_user_pages(uint32_t pg_cnt);

// 安装1页大小的vaddr,专门针对fork时虚拟地址位图无须操作的情况
void *get_a_page_without_op_vaddrbitmap(enum pool_flags pf, uint32_t vaddr);

// 初始化内存
void block_desc_init(struct mem_block_desc *desc_array);

// 在堆中申请size字节内存
void *sys_malloc(uint32_t size); 

// 释放内存
// 释放以虚拟地址vaddr为起始的cnt个物理页框
void mfree_page(enum pool_flags pf, void *_vaddr, uint32_t pg_cnt); 

// 将物理地址pg_phy_addr回收到物理内存池
void pfree(uint32_t pg_phy_addr); 

// 系统调用实现释放内存
void sys_free(void *ptr);

// 根据物理页框地址pg_phy_addr在相应的内存池的位图清0,不改动页表
void free_a_phy_page(uint32_t pg_phy_addr);


#endif // __KERNEL_MEMORY_H