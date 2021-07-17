  %include "boot.inc"
  section loader  vstart=LOADER_BASE_ADDR                    ; 注意这里必须连着不然编译器会报错
  LOADER_STACK_TOP      equ   LOADER_BASE_ADDR
; 构建GDT以及内部的描述符                             
  GDT_BASE:             dd    0x0000_0000                    ; dd是伪指令,意为define double-world，即定义双字变量
                        dd    0x0000_0000                    ; 这里其实就是GDT的起始地址，第一个描述符为空

; 代码段描述符，一个dd为4字节，段描述符为8字节，上面为低4字节
  CODE_DESC:            dd    0x0000_FFFF                         
                        dd    DESC_CODE_HIGH4
; 栈堆描述符,和数据段共用
  DATA_STACK_DESC:      dd    0x0000_FFFF                    
                        dd    DESC_DATA_HIGH4

; 显存描述符，非平坦
  VIDEO_DESC:           dd    0x8000_0007                    ; limit(0xbffff - 0xb8000)/4k = 0x7
                        dd    DESC_VIDEO_HIGH4               ; 此时dp1为0
            
  GDT_SIZE              equ   $ - GDT_BASE                   ; 先通过地址差来获得GDT的大小，进而GDT大小减1得到了段界限
  GDT_LIMIT             equ   GDT_SIZE - 1                   ; 这是为加载GDT做准备
  times 60 dq 0                                              ; 循环60次，以此来留60个描述符的空位
  SELECTOR_CODE         equ   (0x0001 << 3) + TI_GDT + RPL0  ; (CODE_DESC - GDT_BASE)/8 + TI_GDT + RPL0
  SELECTOR_DATA         equ   (0x0002 << 3) + TI_GDT + RPL0  ; 同上，数据段和栈堆是统一描述符
  SELECTOR_VIDEO        equ   (0x0003 << 3) + TI_GDT + RPL0  ; 同上，22~24构建代码段、数据段、显存段的选择子

  ; total_mem_bytes用于保存内存容量，以字节为单位
  ; 当前偏移loader.bin文件头0x200字节，loader.bin的加载地址是0x900
  ; 故total_mem_bytes内存中的地址0xb00,将来在内核中咱们会引用此地址
  total_mem_bytes       dd    0                              ; 此变量用于存储获取到的内存容量，以字节为单位
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; 以下是gdt的指针，前2字节是gdt界限，后4字节是gdt起始地址
  gdt_ptr               dw    GDT_LIMIT                      ; 该指针式lgdt加载GDT到gdtr寄存器中使用
                        dd    GDT_BASE                       ; lgdt48为内存数据
  
; 人工对齐:total_mem_bytes4+gdt_ptr6+ards_buf244+ards_nr2,共256字节
  ards_buf times 244 db 0                                    ; 提前定义的1缓冲区，存储BIOS 0x15中断0xe820子功能返回的ARDS结构
  ards_nr dw 0                                               ; 用来记录ARDS结构体数量

  loader_start:

;----------------------  int 15h eax = 0000E820h,edx = 534D4150h('SMAP')获取内存布局  ---------------------------------

    xor                 ebx,  ebx                            ; 第一次调用时,ebx值为0
    mov                 edx,  0x534d4150                     ; edx只赋值一次,循环体中不变
    mov                  di,  ards_buf                       ; ards结构缓冲区
.e820_mem_get_loop:                                          ; 循环获取每个ARDS内存范围描述结构
    mov                 eax,  0x0000e820                     ; 执行int 0x15后，eax值变为0x534d4150,所以每次执行int前都要更新为子功能号
    mov                 ecx,  20                             ; ARDS地址范围描述符结构大小是20字节
    int                 0x15                      
    jc .e820_failed_so_try_e801                              ; 若cf位为1则有错误发生，尝试0xe801子功能
    add                  di,  cx                             ; 使di增加20字节指向缓冲区中新的ARDS结构位置
    inc word [ards_nr]                                       ; 记录ARDS数量
    cmp                 ebx,  0                              ; 若ebx为0且cf不为1，这说明ards以及全部返回且为最后一个
    jnz .e820_mem_get_loop

; 在所有ards结构中，找出[base_add_low + length_low]的最大值，即为内存容量
    mov                  cx,  [ards_nr]                      ; 遍历每一个ARDS结构体，循环次数是ARDS的数量
    mov                 ebx,  ards_buf           
    xor                 edx,  edx                            ; edx为最大的内存容量，在此先清0
.find_max_mem_area:                                          ; 无需判断type是否为1，最大的内存块一定是可以被使用的
    mov                 eax,  [ebx]                          ; base_add_low
    add                 eax,  [ebx+8]                        ; length_low
    add                 ebx,  20                             ; 指向缓冲区下一个ARDS结构
    cmp                 edx,  eax                            ; 冒泡排序，找出最大，edx寄存器始终是最大的内存容量
    jge .next_ards
    mov                 edx,  eax                            ; edx为总内存的大小
.next_ards:
    loop .find_max_mem_area
    jmp  .mem_get_ok                                         ; 

;--------------------------  int 15h ax = E801h 获取内存的大小，最大支持4G  -----------------------------------------
; 返回后，ax cx的值一样，以KB为单位，bx dx值一样，以64KB为单位
; 在ax和cx寄存器中低16MB，在bx和dx寄存器中为16MB到4GB
.e820_failed_so_try_e801:
    mov                  ax,  0xe801                         ; 
    int                 0x15
    jc .e801_failed_so_try88                                 ; 若当前e801方法失败，就尝试0x88方法

; 1 先算出低15MB的内存，ax 和 cx中是以KB为单位的内存数量，将其转换为以byte为单位
    mov                  cx,  0x400                          ; cx 和 ax值一样，cx用作乘数
    mul                  cx
    shl                 edx,  16                             ; 
    and                 eax,  0x0000_FFFF                    ;
    or                  edx,  eax                            ;
    add                 edx,  0x1000_00                      ; ax只是15MB，故要加1MB
    mov                 esi,  edx                            ; 先把低15MB的内存容量存入esi寄存器备份

; 2 再将16MB以上的内存转变为byte为单位，寄存器bx和dx中是以64KB为单位的内存数量 
    xor                 eax,  eax                            ;
    mov                  ax,  bx                             ;
    mov                 ecx,  0x1000_0                       ; 0x10000十进制为64KB
    mul                 ecx                                  ; 32位乘法，默认的被乘数是eax，积位64位，高32位存入edx，低32位存入eax
    add                 esi,  eax                            ; 由于此方法只能测4GB以内的内存，故32位eax即可，edx肯定为0，只加eax即可
    mov                 edx,  esi                            ; edx位总内存大小
    jmp .mem_get_ok

;--------------------------  int 15h ax = 0x88 获取内存的大小，只能获取64MB以内  -----------------------------------------
.e801_failed_so_try88:
    ; int 15后，ax存入的是以KB位单位的内存容量
    mov                  ah,  0x88                           ;
    int                  0x15                                ;
    jc .error_hlt
    and                 eax,  0x0000_FFFF                    ;

    ; 16位乘法，被乘数是ax,积为32位.积的高16位在dx中，积的低16位在ax中
    mov                  cx,  0x400                          ; 0x400等于1024，将ax中的内存容量换位以btype位单位
    mul                  cx                                  ;
    shl                 edx,  16                             ; 把dx移到高16位
    or                  edx,  eax                            ; 把积的低16位组合到edx，为32为的积
    add                 edx,  0x1000_00                      ; 0x88子功能只会返回1MB以上的内存，故实际内存大小要加上1MB

.mem_get_ok:
    mov   [total_mem_bytes],  edx                            ; 内存换位byte单位后存入total_mem_bytes处


;------------------------------------------   准备进入保护模式   --------------------------------------------------=-------
;1 打开A20
;2 加载gdt
;3 将cr0的pe位置1

    ;---------------------------------------------  打开A20  --------------------------------------------------------------
    in                   al,  0x92
    or                   al,  0000_0010B
    out                0x92,  al

    ;---------------------------------------------  加载GDT  --------------------------------------------------------------
    lgdt                [gdt_ptr]

    ;---------------------------------------------  cr0第0位置  -----------------------------------------------------------
    mov                 eax,  cr0
    or                  eax,  0x0000_0001
    mov                 cr0,  eax

    jmp dword SELECTOR_CODE:p_mode_start                     ; 刷新流水线，避免分支预测的影响,这种cpu优化策略，最怕jmp跳转，
                                                             ; 这将导致之前做的预测失效，从而起到了刷新的作用。
.error_hlt:                                                  ; 出错则挂起
    hlt

[bits 32]
p_mode_start:
    mov                  ax,  SELECTOR_DATA                  ; start,用于选择子初始化成各段寄存器
    mov                  ds,  ax
    mov                  es,  ax
    mov                  ss,  ax
    mov                 esp,  LOADER_STACK_TOP
    mov                  ax,  SELECTOR_VIDEO
    mov                  gs,  ax                             ; end

;-----------------------------------------------  加载kernel  ------------------------------------------------------------
    mov                 eax,  KERNEL_START_SECTOR            ; kernel.bin所在的扇区号
    mov                 ebx,  KERNEL_BIN_BASE_ADDR           ; 从磁盘读出，写入到ebx指定的地址
    mov                 ecx,  200                            ; 读入的扇区数

    call rd_disk_m_32                                        ; 用于从硬盘中读文件 

    ; 创建页目录及页表并初始化页内存位图
    call setup_page
   
    ; 要将描述符地址及偏移量写入内存gdt_ptr，一会用新地址重新加载,为了重启加载GDT作准备
    sgdt [gdt_ptr]                                           ; 存储到原来gdt所有的位置

    ; 将gdt描述符中视频段描述符中的段基址+0xc0000_000，修改显存段的段描述符的段基址
    mov                 ebx,  [gdt_ptr + 2]
    or   dword [ebx+0x18+4],  0xc0000_000                    ; 视频段是第3个段描述符，每个描述符是8字节，故0x18
    ; 段描述符的高4字节的最高位为段基址的第31～24位

    ; 将gdt的基址加上0xc0000_000使其成为内核所在的高地址
    add  dword [gdt_ptr+2], 0xc0000_000                      ; gdt_ptr处的值包括两个部分，dasasdasdasdsaasd`sd前部分2字节大小的偏移量

    add                 esp,  0xc0000_000                    ; 将栈指针同样映射带内核地址

    ; 把页目录地址赋给cr3
    mov                 eax,  PAGE_DIR_TABLE_POS             ; 开启分页机制的第二步和第三步
    mov                 cr3,  eax                            ; CR3->TLB

    ; 打开cr0的pg位（第31位）
    mov                 eax,  cr0                            ;
    or                  eax,  0x8000_0000
    mov                 cr0,  eax                            ;

    ; 开启分页后，用gdt新的地址重新加载
    lgdt [gdt_ptr]                                           ; 重新加载

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;  此时不刷新流水线也没问题  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 由于一直处在32位下,原则上不需要强制刷新,经过实际测试没有以下这两句也没问题.
; 但以防万一，还是加上啦，免得将来出来莫句奇妙的问题.
    jmp  SELECTOR_CODE:enter_kernel                          ;
enter_kernel:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    call kernel_init
    mov                 esp,  0xc009f_000
    jmp  KERNEL_ENTRY_POINT                                  ; 用地址0x1500访问测试，结果ok


;-------------------------------------- 将kernel.bin中的segment拷贝到编译的地址 -----------------------------------
kernel_init:
    xor                eax,  eax                             ;
    xor                ebx,  ebx                             ; ebx记录程序头表地址
    xor                ecx,  ecx                             ; ecx记录程序头表中的program_header数量
    xor                edx,  edx                             ; edx记录program header的尺寸，即e_phentsize

    mov                 dx,  [KERNEL_BIN_BASE_ADDR + 42]     ; 偏移文件42字节处的属性是e_phentsize，表示program_header大小
    mov                ebx,  [KERNEL_BIN_BASE_ADDR + 28]     ; 偏移文件开始部分28字节的地方是e_phoff，表示第一个program header在文件中的偏移量
    ; 其实该值是0x34，不过还是谨慎一点，这里来读取实际值
    add                ebx,  KERNEL_BIN_BASE_ADDR            ;
    mov                 cx,  [KERNEL_BIN_BASE_ADDR + 44]     ; 偏移文件开始部分44字节的地方是e_phnum,表示有几个program header
.each_segment:
    cmp     byte [ebx + 0],  PT_NULL                         ; p_type等于PT_NULL, 说明此program header未使用
    je .PTNULL                                               ; 空程序类型

    ; 为函数memcpy压入参数,参数是从右往左依然压入.函数原型类似于 memcpy(dst,src,size)，通过mem_cpy函数复制到段自身的虚拟地址处
    push dword [ebx + 16]                                    ; program header中偏移16字节的地方是p_filesz,压入函数memcpy的第三个参数:size
    mov                eax,  [ebx + 4]                       ; 距程序头偏移量为4字节的位置是p_offset
    add                eax,  KERNEL_BIN_BASE_ADDR            ; 加上kernel.bin被加载到的物理地址,eax为该段的物理地址
    push               eax                                   ; 压入函数memcpy的第二个参数:源地址
    push             dword   [ebx + 8]                       ; 压入函数memcpy的第一个参数:目的地址,偏移程序头8字节的位置是p_vaddr，这就是目的地址
    call mem_cpy                                             ; 调用mem_cpy完成段复制
    add                esp,  12                              ; 清理栈中压入的三个参数
.PTNULL:                                                     ; 
    add                ebx,  edx                             ; edx为program header大小,即e_phentsize,在此ebx指向下一个program header
    loop .each_segment
    ret

;-----------------------------------------------  逐字节拷贝 mem_cpy(dst,src,size)   ---------------------------------
;输入:栈中三个参数(dst,src,size)
;输出:无
;--------------------------------------------------------------------------------------------------------------------
mem_cpy:
    cld                                                      ; 用于清除方向标志，让数据的源地址和目的地址逐渐增大
    push               ebp
    mov                ebp,  esp                             ; 
    push               ecx                                   ; rep指令用到了ecx，但ecx对于外层段的循环还有用，故先入栈备份
    mov                edi,  [ebp + 8]                       ; destination，复制工作所需要条件初始化
    mov                esi,  [ebp + 12]                      ; source
    mov                ecx,  [ebp + 16]                      ; size
    rep                movsb                                 ; 逐字节拷贝，准备工作完成开始做事

    ; 恢复环境
    pop                ecx
    pop                ebp
    ret                                                      ; 回到216行


;-----------------------------------------    创建页目录及页表  ----------------------------------------------------
setup_page:
; 先把页目录占有的空间逐字节清0
    mov                ecx,  4096                            ;
    mov                esi,  0
.clear_page_dir:
    mov  byte [PAGE_DIR_TABLE_POS + esi], 0                  ;
    inc                esi
    loop .clear_page_dir

; 开始创建页目录（PDE）
.create_pde:                                                 ; 创建Page Directory Entry
    mov                eax,  PAGE_DIR_TABLE_POS
    add                eax,  0x1000                          ; 此时eax为第一个页表的位置及属性
    mov                ebx,  eax                             ; 此处为ebx赋值，是为.create_pte作准备,ebx为基址

; 下面将页目录项0和0xc00都存为第一个页表的地址，每个页表表示4MB内存 
; 这样0xc03f_ffff以下的地址和0x003f_ffff以下的地址都指向相同的页表
; 这是为将地址映射为内核地址作准备
    or                 eax,  PG_US_U | PG_RW_W | PG_P        ; 页目录项的属性RW和P位为1，US为1，表示用户属性，所有特权级别都可以被访问
    mov [PAGE_DIR_TABLE_POS + 0x0]  , eax                    ; 在页目录表中的第1个目录项写入第一个页表的位置(0x101000)及属性(7)，一个页表项占用4字节
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax                    ; 0xc00表示第768个页表占用的目录项，0xc00以上的目录项用于内核空间
    ; 也就是页表的0xc000_0000~0xffff_ffff 共计1G属于内核，0x0~0xbfff_ffff共计3G属于用户进程
    sub                eax,  0x1000                          ; 寄存器eax是页目录项的内容，分别存入页目录项的第0项和第768项
    mov [PAGE_DIR_TABLE_POS + 4092] , eax                    ; 使得最后一个目录项指向页目录表自己的地址
                                                             ; 204~205行的目的是在页目录的最后一个页目录项写入页表自己的物理地址
; 下面创建页表项(PTE)
    mov                ecx,  256                             ; 1MB低端内存 / 每页大小 4K = 256
    mov                esi,  0
    mov                edx,  PG_US_U | PG_RW_W | PG_P        ; 属性为7，US=1，RW=1，P=1
.create_pte:                                                 ; 创建Page Table Entry
    mov      [ebx + esi*4],  edx                             ; 此时ebx已经在上面通过eax赋值为0x1010_00，也就是第一个页表的地址
    add                edx,  4096
    inc                esi 
    loop .create_pte

; 创建内核其他页表的PDE
    mov                eax,  PAGE_DIR_TABLE_POS              ; 创建除第768个页表之外的其他页表对应的PDE，也是内核空间除第0个页表外的其余所有页表对应的目录项
    add                eax,  0x2000                          ; 此时eax为第二个页表
    or                 eax,  PG_US_U | PG_RW_W | PG_P        ; 页目录的属性US=1，RW=1，P=1
    mov                ebx,  PAGE_DIR_TABLE_POS
    mov                ecx,  254                             ; 范围为第769～1022的所有目录项数目
    mov                esi,  769
.create_kernel_pde:
    mov        [ebx+esi*4],  eax
    inc                esi
    add                eax,  0x1000
    loop .create_kernel_pde
    ret

;--------------------------------------------------------------------------------------------------------
; 功能:读取硬盘n个扇区,和之前16位差不多
rd_disk_m_32:	   
;--------------------------------------------------------------------------------------------------------
                                                             ; eax=LBA扇区号
                                                             ; ebx=将数据写入的内存地址
                                                             ; ecx=读入的扇区数
    mov                esi,  eax                             ; 备份eax
    mov                 di,  cx	                             ; 备份扇区数到di
; 读写硬盘:
; 第1步：设置要读取的扇区数
    mov                 dx,  0x1f2
    mov                 al,  cl
    out                 dx,  al                              ; 读取的扇区数

    mov                eax,  esi                             ; 恢复ax

; 第2步：将LBA地址存入0x1f3 ~ 0x1f6

    ; LBA地址7~0位写入端口0x1f3
    mov                 dx,  0x1f3                       
    out                 dx,  al                          

    ; LBA地址15~8位写入端口0x1f4
    mov                 cl,  8
    shr                eax,  cl
    mov                 dx,  0x1f4
    out                 dx,  al

    ; LBA地址23~16位写入端口0x1f5
    shr                eax,  cl
    mov                 dx,  0x1f5
    out                 dx,  al

    shr                eax,  cl
    and                 al,  0x0f	                         ; lba第24~27位
    or                  al,  0xe0	                         ; 设置7～4位为1110,表示lba模式
    mov                 dx,  0x1f6
    out                 dx,  al

; 第3步：向0x1f7端口写入读命令，0x20 
    mov                 dx,  0x1f7
    mov                 al,  0x20                        
    out                 dx,  al

;;;;;;; 至此,硬盘控制器便从指定的lba地址(eax)处,读出连续的cx个扇区,下面检查硬盘状态,不忙就能把这cx个扇区的数据读出来

; 第4步：检测硬盘状态
  .not_ready:                                                ; 测试0x1f7端口(status寄存器)的的BSY位
    ; 同一端口,写时表示写入命令字,读时表示读入硬盘状态
    nop
    in                  al,  dx
    and                 al,  0x88                            ; 第4位为1表示硬盘控制器已准备好数据传输,第7位为1表示硬盘忙
    cmp                 al,  0x08
    jnz .not_ready                                           ; 若未准备好,继续等。

;第5步：从0x1f0端口读数据
    mov                 ax,  di                              ; 以下从硬盘端口读数据用insw指令更快捷,不过尽可能多的演示命令使用,
                                                             ; 在此先用这种方法,在后面内容会用到insw和outsw等

    mov                 dx,  256                             ; di为要读取的扇区数,一个扇区有512字节,每次读入一个字,共需di*512/2次,所以di*256
    mul                 dx
    mov                 cx,  ax	   
    mov                 dx,  0x1f0
  .go_on_read:
    in                  ax,  dx		
    mov              [ebx],  ax
    add                ebx,  2
	; 由于在实模式下偏移地址为16位,所以用bx只会访问到0~FFFFh的偏移。
	; loader的栈指针为0x900,bx为指向的数据输出缓冲区,且为16位，
	; 超过0xffff后,bx部分会从0开始,所以当要读取的扇区数过大,待写入的地址超过bx的范围时，
	; 从硬盘上读出的数据会把0x0000~0xffff的覆盖，
	; 造成栈被破坏,所以ret返回时,返回地址被破坏了,已经不是之前正确的地址,
	; 故程序出会错,不知道会跑到哪里去。
	; 所以改为ebx代替bx指向缓冲区,这样生成的机器码前面会有0x66和0x67来反转。
	; 0X66用于反转默认的操作数大小! 0X67用于反转默认的寻址方式.
	; cpu处于16位模式时,会理所当然的认为操作数和寻址都是16位,处于32位模式时,
	; 也会认为要执行的指令是32位.
	; 当我们在其中任意模式下用了另外模式的寻址方式或操作数大小(姑且认为16位模式用16位字节操作数，
	; 32位模式下用32字节的操作数)时,编译器会在指令前帮我们加上0x66或0x67，
	; 临时改变当前cpu模式到另外的模式下.
    ; 假设当前运行在16位模式,遇到0X66时,操作数大小变为32位.
    ; 假设当前运行在32位模式,遇到0X66时,操作数大小变为16位.
	; 假设当前运行在16位模式,遇到0X67时,寻址方式变为32位寻址
	; 假设当前运行在32位模式,遇到0X67时,寻址方式变为16位寻址.

    loop .go_on_read
    ret
