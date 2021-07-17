#ifndef __DEVICE_CONSOLE_H
#define __DEVICE_CONSOLE_H
#include "stdint.h"

// 初始化终端
void console_init(void);

// 获取终端
void console_acquire(void);

// 释放终端
void console_release(void);

// 终端中输出字符串
void console_put_str(char *str);

// 终端中输出字符
void console_put_char(uint8_t char_asci);

// 终端中输出16进制整数
void console_put_int(uint32_t num);

#endif // __DEVICE_CONSOLE_H