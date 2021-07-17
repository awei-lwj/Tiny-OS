/**
 * 为print.S提供方便引用的头文件定义.
 */
#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H
#include "stdint.h"
void put_char(uint8_t char_asci);
void put_str(char *message);
void put_int(uint32_t num); // 以16进制打印
void set_cursor(uint32_t cursor_pos);
void cls_screen(void);


#endif // __LIB_KERNEL_PRINT_H
