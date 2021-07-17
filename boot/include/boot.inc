;------------------------------------------	 loader和kernel   --------------------------------------------

LOADER_BASE_ADDR     equ  0x900                  ; 定义了loader在内存中的位置,loader
LOADER_STACK_TOP     equ  LOADER_BASE_ADDR       ; 栈指针 
LOADER_START_SECTOR  equ  0x2                    ; loader在硬盘中的逻辑扇区地址 ,LBA addresses

KERNEL_BIN_BASE_ADDR equ  0x7000_0               ; kernel加载的地方
KERNEL_START_SECTOR  equ  0x9
KERNEL_ENTRY_POINT   equ  0xc000_1500

;-------------------------------------------   gdt描述符属性  --------------------------------------------
DESC_G_4K            equ  1_0000_0000_0000_0000_0000_000b      ; 描述符的G位为4k粒度
DESC_D_32            equ  1_0000_0000_0000_0000_0000_00b       ; 用来指示有效地址
DESC_L	             equ  0_0000_0000_0000_0000_000_00b	       ; 64位代码标记，此处标记为0便可。
DESC_AVL             equ  0_0000_0000_0000_0000_000_0b         ; cpu不用此位，暂置为0  
DESC_LIMIT_CODE2     equ  1111_0000_0000_0000_0000_b           ; 代码段的段界限的第二部分
DESC_LIMIT_DATA2     equ  DESC_LIMIT_CODE2                     ; 数据段的段界限的第二部分
DESC_LIMIT_VIDEO2    equ  0000_0000_0000_0000_000b             ; video段的段界限的第二部分
DESC_P	             equ  1_0000_0000_0000_000b                ; Present，段是否存
DESC_DPL_0           equ  00_0000_0000_0000_0b                 ; DPL字段，描述符号特权级别
DESC_DPL_1           equ  01_0000_0000_0000_0b                 ; 特权级别1
DESC_DPL_2           equ  10_0000_0000_0000_0b                 ; 特权级别2
DESC_DPL_3           equ  11_0000_0000_0000_0b                 ; 特权级别3
DESC_S_CODE          equ  1_0000_0000_0000_b                   ; 代码段的S位，此值为1，表示为普通的内存段
DESC_S_DATA          equ  DESC_S_CODE                          ; 数据段的S位
DESC_S_sys           equ  0_0000_0000_0000_b                   ; 系统段的S位
DESC_TYPE_CODE       equ  1000_0000_0000_b                     ; 定义了一个代码段的type
DESC_TYPE_DATA       equ  0010_0000_0000_b                     ; 定义了一个数据段的type

; DESC_TYPE_CODE x=1,c=0,r=0,a=0 代码段是可执行的,非依从的,不可读的,已访问位a清0.
; DESC_TYPE_DATA x=0,e=0,w=1,a=0 数据段是不可执行的,向上扩展的,可写的,已访问位a清0.
DESC_CODE_HIGH4     equ  (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_CODE2  + DESC_P + DESC_DPL_0 + DESC_S_CODE + DESC_TYPE_CODE + 0x00
DESC_DATA_HIGH4     equ  (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_DATA2  + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x00
DESC_VIDEO_HIGH4    equ  (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x0b

;--------------------------------------------   选择子属性  --------------------------------------------------
RPL0                equ   00b
RPL1                equ   01b
RPL2                equ   10b
RPL3                equ   11b
TI_GDT	            equ   000b
TI_LDT	            equ   100b

;---------------------------------------------   页表配置   ----------------------------------------------
PAGE_DIR_TABLE_POS   equ  0x1000_00

;-------------------------------------------   页表相关属性    -----------------------------------------------
PG_P                equ   1b
PG_RW_R	            equ  00b 
PG_RW_W	            equ  10b 
PG_US_S	            equ  000b 
PG_US_U	            equ  100b 


;----------------------------------------  program type 定义   ---------------------------------------------
PT_NULL            equ  0

