#ifndef __LIB_STRING_H
#define __LIB_STRING_H
#include "stdint.h"

// 将destination起始的size个字节置为value
void     memset (void* dest, uint8_t value, uint32_t size);

// 将source起始的size个字节复制到destination 
void     memcpy (void* dest, const void* src, uint32_t size);

// 连续比较以first和地址second开头的size个字节,若相等则返回0,若first大于second,返回+1,否则返回-1
int      memcmp (const void* first, const void* second, uint32_t size);
 
// 将字符串从source复制到destination
char*    strcpy (char* dest, const char* src);

// 返回str字符串长度
uint32_t strlen (const char* str);

// 比较两个字符串,若first中的字符大于second中的字符返回1,相等时返回0,否则返回-1.
int8_t   strcmp (const char *first, const char *second); 

// 从左到右查找字符串str中首次出现字符ch的地址(不是下标,是地址)
char*    strchr (const char* string, const uint8_t ch);

// 从后往前查找字符串str中首次出现字符ch的地址(不是下标,是地址)
char*    strrchr(const char* string, const uint8_t ch);

// 将字符串source拼接到destination后,将回拼接的串地址
char*    strcat (char* dest, const char* src);

// 在字符串str中查找指定字符ch出现的次数
uint32_t strchrs(const char* str, uint8_t ch);


#endif //  __LIB_STRING_H
