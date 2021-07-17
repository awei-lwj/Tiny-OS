#include "timer.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/print.h"
#include "../kernel/debug.h"
#include "../kernel/interrupt.h"
#include "../thread/thread.h"

#define IRQ0_FREQUENCY 100
/**
 * @brief 
 * 8253时钟寄存器的方式共有六种
 * 1. 计数结束中断方式(Interrupt on Terminal Count)
 * 2. 硬件可重触发单稳方式(Hardware Retrigger One-Shot)
 * 3. 比率发生器(Rate Generator)   -------------------------> 选择的方式
 * 4. 方波发生器(Square Wave Generator)
 * 5. 硬件触发选通(Hardware Trigger Strobe)
 * 6. 软件触发选通(Software Triggered Strobe)
 * 
 * 这里我们采用第3种
 */

#define IRQ0_FREQUENCY 100                // 时钟中断的频率，我们要将它设为100Hz
#define INPUT_FREQUENCY 1193180           // 计数器0的工作脉冲信号频率(CLK引脚上的时钟脉冲信号的频率未一秒钟1193180次)
#define COUNTER0_VALUE INPUT_FREQUENCY / IRQ0_FREQUENCY
#define CONTRER0_PORT 0x40                // 计数器0所在的端口
#define COUNTER0_NO 0                     // 控制字中选择计数器的号码
#define COUNTER_MODE 2                    // 工作模式的代码,，其值为2，即方式2，这是我们选择的工作方式：比率发生器。
#define READ_WRITE_LATCH 3                // 是读写方式，其值为3，先读写低8位，再读写高8位
#define PIT_CONTROL_PORT 0x43             // 控制器端口0x43中写入控制字

#define mil_seconds_per_intr (1000 / IRQ0_FREQUENCY)

uint32_t ticks; // ticks是内核自中断开启以来总共的时间

// 把操作的计数器counter_no、读写锁属性rwl、计数器模式counter_mode、写入模式控制寄存器、并赋予初始值counter_value
static void frequency_set(uint8_t counter_port,
                          uint8_t counter_no,
                          uint8_t rwl,
                          uint8_t counter_mode,
                          uint16_t counter_value)
{
    // 往控制字寄存器端口0x43中写入控制字
    outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));

    // 先写入counter_value的低8位
    outb(counter_port, (uint8_t)counter_value);

    // 再写入counter_value的高8位
    outb(counter_port, (uint8_t)counter_value >> 8);

    return;
}

// 以tick为单位的sleep,任何时间形式的sleep会转换此ticks形式
static void ticks_to_sleep(uint32_t sleep_ticks)
{
    uint32_t start_tick = ticks;

    // 若间隔的ticks数不够便让出cpu
    while (ticks - start_tick < sleep_ticks)
    {
        thread_yield();
    }

    return;
}

// 以毫秒为单位的sleep
void mtime_sleep(uint32_t m_seconds)
{
    uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);

    ASSERT(sleep_ticks > 0);
    ticks_to_sleep(sleep_ticks);

    return;
}

// 时钟中断的处理函数
static void intr_timer_handler(void)
{
    struct task_struct *cur_thread = running_thread();

    ASSERT(cur_thread->stack_magic == 0x20000720);      // 检查栈是否溢出

    cur_thread->elapsed_ticks++;                        // 记录此线程占用的cpu时间嘀
    ticks++;                     //从内核第一次处理时间中断后开始至今的滴哒数,内核态和用户态总共的时间

    if (cur_thread->ticks == 0)  // 若进程时间片用完就开始调度新的进程上cpu
    {
        schedule();
    }
    else                         // 将当前进程的时间片-1
    {           
        cur_thread->ticks--;
    }

    return;
}

// 初始化PIT8253
void timer_init(void)
{
    put_str("timer_init start\n");

    //设置8253的定时周期,也就是发中断的周期
    frequency_set(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
    
    register_handler(0x20, intr_timer_handler);
    put_str("timer_init done\n");

    return;
}
