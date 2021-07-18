# Error correction

## 1. /boot/include/boot.inc

```x86asm
DESC_CODE_HIGH4        equ     (0x00 << 24)   + DESC_G_4K  + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_CODE2  + DESC_P + DESC_DPL_0 + DESC_S_CODE + DESC_TYPE_CODE + 0x00    


DESC_DATA_HIGH4        equ     (0x00 << 24)   + DESC_G_4K  + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_DATA2  + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x00  
                          
DESC_VIDEO_HIGH4       
equ     (0x00 << 24)   + DESC_G_4K  + DESC_D_32 + DESC_L + DESC_AVL + 
DESC_LIMIT_VIDEO2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x00
                                                                         -> &

This place is wrong,you should change it as following:
DESC_VIDEO_HIGH4    
equ  (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 
+ DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x0b
                                                       -> &
```

## 第六章的一些问题

[操作系统真象还原 -- ld: i386 架构于输入文件 lib/kernel/print.o 与 i386:x86-64 输出不兼容](https://blog.csdn.net/shuzishij/article/details/87812133)

```c
Q:
awei@ubuntu:~/Desktop/my-operation-system$ gcc -m32 -I lib/kernel/ -c -o kernel/main.o kernel/main.c 
In file included from /usr/lib/gcc/x86_64-linux-gnu/9/include/stdint.h:9,
                 from lib/kernel/print.h:3,
                 from kernel/main.c:1:
/usr/include/stdint.h:26:10: fatal error: bits/libc-header-start.h: 没有那个文件或目录
   26 | #include <bits/libc-header-start.h>
      |          ^~~~~~~~~~~~~~~~~~~~~~~~~~
```

```c

Solution:
sudo apt-get install gcc-multilib

```

```c
Q:
awei@ubuntu:~/Desktop/my-operation-system$ ld -Ttext 0xc0001500 -e main -o kernel.bin kernel/main.o lib/kernel/print.o
ld: i386 architecture of input file `lib/kernel/print.o' is incompatible with i386:x86-64 output

Solution:
nasm -f elf -o lib/kernel/print.o lib/kernel/print.S
gcc -m32 -I lib/kernel/ -c -o kernel/main.o kernel/main.c 
ld -m elf_i386 -Ttext 0xc0001500 -e main -o kernel.bin kernel/main.o lib/kernel/print.o
Solution:

```

```c
Question:
awei@ubuntu:~/Desktop/my-operating-system$ ld -m elf_i386 -Ttext 0xc0001500 -e main -o bin/kernel.bin build/main.o build/init.o build/interrupt.o build/print.o build/kernel.o
ld: build/interrupt.o: in function `idt_init':
interrupt.c:(.text+0x404): undefined reference to `__stack_chk_fail_local'
ld: bin/kernel.bin: hidden symbol `__stack_chk_fail_local' isn't defined
ld: 最后的链结失败: bad value

Solution:
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/interrupt.o kernel/interrupt.c -fno-stack-protector
```

```c
Q:
未定义标识符 "NULL"C/C++(20)
// 将destination起始的size个字节置为value
void memset(void* dest, uint8_t value, uint32_t size)
{
    ASSERT(dest != NULL);
    uint8_t* destination = (uint8_t*) dest;
    while(size-- > 0){
        *destination++ = value;
    }

}

S: 不能引入stdio.h
NULL = 0
// 将destination起始的size个字节置为value
void memset(void* dest, uint8_t value, uint32_t size)
{
    ASSERT(dest != 0);
    uint8_t* destination = (uint8_t*) dest;
    while(size-- > 0){
        *destination++ = value;
    }

}


```

```c
void memset(void* dest, uint8_t value, uint32_t size)
{
    /**
     * @brief 
     * 内存区域的数据初始化
     * 逐字节地把 value 写入起始内存地址为dest的size空间，在本系统中通常用于内存分配时的数据清0
     */

    ASSERT(dest != 0); -> 应该用NULL但是要引入别的头文件用0代替
    uint8_t* destination = (uint8_t*) dest;

    while(size-- > 0){
        *destination++ = value;
    }

    return ;

}
