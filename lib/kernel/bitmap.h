#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H
#include "../kernel/global.h"


#define BITMAP_MASK 1

struct bitmap
{
    /**
     * @brief 
     * 在遍历位图时,整体上以字节为单位,细节上是以位为单位,所以此处位图的指针必须是单字节
     * 
     */

    
    uint32_t btmp_bytes_len;     // 位图的字节长度
    uint8_t *bits;               // 位图的指针,用来记录上层模块的位图的地址
};

// 将位图初始化
void bitmap_init(struct bitmap *btmp);

// 判断位图btmp中的第bit_idx位是否为1，若为1，则返回为true，否则返回false，此函数是被bitmap_scan中调用的
int bitmap_scan_test(struct bitmap *btmp, uint32_t bit_idx);

// 在位图btmp找到连续的 cnt 个可用位，返回起始空闲位下标，若没找到 cnt 个空闲位，返回-1. 位图中的位，其值为0表示该位对应的资源可用
int bitmap_scan(struct bitmap *btmp, uint32_t cnt);

// bitmap_set 接受3个参数，位图指针 btmp 、位索引 bit_idx 、位值 value ，函数功能是将位图 btmp 中的bit_idx 位设置为 value ，其中 bit_idx 为整个位图中的位索引。
void bitmap_set(struct bitmap *btmp, uint32_t bit_idx, int8_t value);


#endif // __LIB_KERNEL_BITMAP_H