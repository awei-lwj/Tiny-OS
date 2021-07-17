#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "../thread/thread.h"
#include "../thread/sync.h"

#define bufsize 64

// 环形缓冲区
struct ioqueue
{
    /**
     * @brief 
     * 解决，用于输入输出的环形缓冲区的生产者-消费者问题
     * 
     */

    struct lock lock;             // 本缓冲区的锁

    // 生产者,缓冲区不满时就继续往里面放数据,否则就睡眠,此项记录哪个生产者在此缓冲区上睡眠。
    struct task_struct *producer; // 生产者

    // 消费者,缓冲区不空时就继续从往里面拿数据,否则就睡眠,此项记录哪个消费者在此缓冲区上睡眠。
    struct task_struct *consumer; // 消费者

    char buf[bufsize];            // 缓冲区大小
    int32_t head;                 // 队首，数据往队首写入
    int32_t tail;                 // 队尾,数据从队尾处读出
};

// 初始化io队列ioqueue
void ioqueue_init(struct ioqueue *ioq);

// 判断队列已满
bool ioq_full(struct ioqueue *ioq);

// 消费者从ioq队列中获取一个字符
char ioq_getchar(struct ioqueue *ioq);

// 生产者万ioq队列中写入一个字符byte
void ioq_putchar(struct ioqueue *ioq, char byte);

// 返回环形缓冲区中的数据长度
uint32_t ioq_length(struct ioqueue *ioq);



#endif // __DEVICE_IOQUEUE_H
