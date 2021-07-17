#include "console.h"
#include "print.h"
#include "stdint.h"
#include "../thread/thread.h"
#include "../thread/sync.h"


static struct lock console_lock; // 控制台锁

// 初始化终端
void console_init(void)
{
    lock_init(&console_lock);
    return;
}

// 获取终端
void console_acquire(void)
{
    lock_acquire(&console_lock);
    return;
}

// 释放终端
void console_release(void)
{
    lock_release(&console_lock);
    return;
}

// 终端中输出字符串
void console_put_str(char *str)
{
    console_acquire();
    put_str(str);
    console_release();

    return;
}

// 终端中输出字符
void console_put_char(uint8_t char_asci)
{
    console_acquire();
    put_char(char_asci);
    console_release();

    return;
}

// 终端中输出16进制整数
void console_put_int(uint32_t num)
{
    console_acquire();
    put_int(num);
    console_release();

    return;
}