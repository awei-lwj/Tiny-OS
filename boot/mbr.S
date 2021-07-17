; 主引导程序
;------------------------------------------------------------------------------
%include "boot.inc"                      
SECTION    MBR  vstart=0x7c00            ; 0x7c00 MBR被BIOS加载到此处，共512字节
   mov     ax , cs                       ; 5~9   用cs寄存器的值去初始化其他寄存器
   mov     ds , ax
   mov     es , ax
   mov     ss , ax
   mov     fs , ax
   mov     sp , 0x7c00                   ; 初始化内存栈
   mov     ax , 0xb800                   ; 段基址
   mov     gs , ax                       ; 13和14行的作用往gs寄存器里面存入段基址

; 清屏
; 清屏利用0x06号功能，上卷全部行，则可以清屏幕  
;------------------------------------------------------------------------------
; INT 0x10  功能号：0x06  功能描述:上卷窗口
;------------------------------------------------------------------------------
; 输入:
; AH 功能号 =  0x06
; AL       ＝ 上卷的行数（如果为 ，表示全部）
; BH       ＝ 上卷行属性
; (CL, CH) ＝ 窗口左上角的（X,Y）位置
; (DL, DH) ＝ 窗口右下角的（X,Y）位置
; 无返回值：
   mov     ax , 0600h                     
   mov     bx , 0700h
   mov     cx , 0                       ; 左上角： (0 , 0)
   mov     dx , 184fh                   ; 右下角； (80,25) 
                                        ; VGA文本模式下，一行只能容纳80个字符，共25行
                                        ; 下标从0开始，所以0x1，8 = 24，0x4f = 79
   int     10h                          ; int 10h

   ; 输出背景色绿色，前景色红色，并且跳动的字符串''1 MBR"
   mov     byte [gs:0x00],  '1'         ; 35~48行执行的mov操作都是往显存里面写入字符
   mov     byte [gs:0x01],  0xA4        ; A表示绿色背景三所，4表示前景色为红色
                                        ; 35~48行写入MBR
   mov     byte [gs:0x02],  ' '         ; byte，用于制动操作数所占的空间
   mov     byte [gs:0x03],  0xA4
    
   mov     byte [gs:0x04],  'M'
   mov     byte [gs:0x05],  0xA4

   mov     byte [gs:0x06],  'B'
   mov     byte [gs:0x07],  0xA4
   
   mov     byte [gs:0x08],  'R'
   mov     byte [gs:0x09],  0xA4        ; 写入结束

   mov     eax,  LOADER_START_SECTOR    ; 起始扇区lba地址
   mov     bx ,  LOADER_BASE_ADDR       ; 写入地址
   mov     cx ,  4                      ; 待读入的扇区数
   call    rd_disk_m_16                 ; 以下读取程序的起始部分

   jmp     LOADER_BASE_ADDR + 0x300     ; 第123过来的位置

;--------------------------------------------------------------------------------
; 功能： 读取硬盘n个扇区
rd_disk_m_16:                           ; 用寄存器传递参数                    
;--------------------------------------------------------------------------------
                                        ; eax = LBA扇区号
                                        ; bx  = 将数据写入的内存地址
                                        ; cx  = 读入的扇区数
   mov     esi, eax                     ; 备份eax
   mov     di , cx                      ; 备份cx
; 读写硬盘
; 第1步：设置要读取的扇区数                 ; 67~70行,选取一个通道，
   mov     dx , 0x1f2                   ; 再往sector count寄存器中写入扇区数
   mov     al , cl                      ; 
   out     dx , al                      ; 读取的扇区数

   mov     eax, esi                     ; 恢复ax

; 第2步：将LBA地址存入0x1f3 ~ 0x1f6      
                                        ; 74~95将LBA写入三个LBA寄存器和device寄存器中
   ; LBA地址07~00位写入端口0x1f3         
   mov     dx , 0x1f3                   ; LBA low
   out     dx , al

   ; LBA地址15~08位写入端口0x1f4
   mov     cl , 8
   shr     eax, cl
   mov     dx , 0x1f4                   ; LBA mid
   out     dx , al

   ; LBA地址23~16位写入端口0x1f5
   shr     eax, cl
   mov     dx , 0x1f5                   ; LBA high
   out     dx , al

   shr     eax, cl
   and     al , 0x0f                    ; LBA第24~27位
   or      al , 0xe0                    ; 设置07~14位为1110，表示lba模式
   mov     dx , 0x1f6                   
   out     dx , al                      ; end

; 第3步：向0x1f7端口写入读命令，0x20
   mov     dx , 0x1f7                   ; 写入command端口0x1f7
   mov     al , 0x20                    ; 读扇区命令0x20
   out     dx , al

; 第4步：检测硬盘状态
  .not_ready:                           ; 检测status寄存器的BSY位
      ; 同一端口，写时表示写入命令，读时表示读入硬盘状态
      nop                               ; 空操作
      in   al , dx                      ; 将status寄存器的值读入搭配al寄存器中
      and  al , 0x88                    ; 第4位为1表示硬盘控制器一准备好数据传输
      cmp  al , 0x08                    ; 第7位为1表示硬盘忙
      jnz  .not_ready                   ; 若未准备好，继续等待

; 第5步：从0x1f0端口读数据
      mov  ax , di
      mov  dx , 256
      mul  dx 
      mov  cx , ax                      ; di为要读取的扇区数，一个扇区共有512个字节
                                        ; 每次读入一个字，共需di*256
      mov  dx , 0x1f0                   ;
   .go_on_read:
      in   ax , dx
      mov [bx], ax
      add  bx , 2
      loop .go_on_read             
      ret                               ; 执行完之后回到第55行

   times   510-($-$$)  db 0             ; $$本section的起始地址，用0将本扇区剩余空间填充
   db      0x55      , 0xaa             ; 预留的固定字节