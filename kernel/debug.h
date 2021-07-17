#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H

// 打印文件名、行号、函数名、条件并使函数悬停
void panic_spin(char *filename, int line, const char *func, const char *condition);

#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)
/**
 * 
 * #define PANIC(...) panic_spin (__FILE__, __LINE__, __func__, __VA_ARGS__)
 * __VA_ARGS__ 是预处理器所支持的专用标识符,代表所有与省略号相对应的参数. "..."表示定义的宏其参数可变
 * ,其实际为字符串指针
 * 
 * 三个预定义的宏，分别表示被编译的文件名、被编译文件的行号、被编译的函数名
 * 
 */

#ifdef NDEBUG
#define ASSERT(CONDITION) ((void)0)
#else
#define ASSERT(CONDITION)                                                 \
   if (CONDITION)                                                         \
   {                                                                      \
   }                                                                      \
   else                                                                   \
   {                                                                      \
      /* 符号#让编译器将宏的参数转化为字符串字面量 */ \
      PANIC(#CONDITION);                                                  \
   }
#endif //  __NDEBUG

#endif // __KERNEL_DEBUG_H
