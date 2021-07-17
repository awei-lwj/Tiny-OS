#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
#include "stdint.h"

typedef void *intr_handler;
void idt_init(void);

/**
 * 
 * 定义中断的两种状态:
 * INTR_OFF值为0,表示关中断,
 * INTR_ON值为1,表示开中断
 * 
 */

// 中断状态
enum intr_status
{
    INTR_OFF,     // 中断关闭
    INTR_ON       // 中断打开
};

// 状态处理函数
enum intr_status intr_get_status(void);    

// 将中断状态设置为status
enum intr_status intr_set_status(enum intr_status);

// 开中断，并且返回开中断前的状态
enum intr_status intr_enable(void);

// 关中断，并且返回关中断前的状态
enum intr_status intr_disable(void);

// 在中断处理程序数组第vector_no个元素中注册安装中断处理程序function
void register_handler(uint8_t vector_no, intr_handler function);


#endif // __KERNEL_INTERRUPT_H