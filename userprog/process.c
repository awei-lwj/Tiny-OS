#include "process.h"
#include "tss.h"
#include "../device/console.h"
#include "../kernel/global.h"
#include "../kernel/debug.h"
#include "../kernel/memory.h"
#include "../kernel/interrupt.h"
#include "../thread/thread.h"
#include "../lib/string.h"
#include "../lib/kernel/list.h"


extern void intr_exit(void);         // 用户进程进入3特权级别的关键

// 构建用户进程初始上下文信息
void start_process(void *filename)   // 用户程序的名称
{
    /** filename
     * 用户程序肯定是从文件系统上加载到内存的，因此进程名是进程的文件名
     * 
     * 该函数用来创建用户进程filename的上下文，也就是填充用户进程的struct intr_stack，通过假装
     * 
     * 从中断返回的方式，间接使和filename运行
     */

    /**
     * @brief 
     * 由操作系统的程序加载器将用户程序从文件系统读取到内存中， 再根据程序文件的格式解析其内容
     * 将程序中的段展开到相应的内存地址
     * 
     */
    void *function          = filename;
    struct task_struct *cur = running_thread();

    // 关键点2： 必须提前准备号用户进程所用的栈结构，start
    // 使得指针self_kstack跨过struct thread_stack栈，最终指向struct intr_stack栈的最低处
    cur->self_kstack += sizeof(struct thread_stack);
    struct intr_stack *proc_stack = (struct intr_stack *)cur->self_kstack;

    // 对栈通用的八个寄存器初始化
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
    proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax       = 0;

    proc_stack->gs  = 0;             // 用户态用不上,直接初始为0
    proc_stack->ds  = proc_stack->es  = proc_stack->fs = SELECTOR_U_DATA;

    // 待执行的用户程序地址
    proc_stack->eip = function;
    // end

    // 关键点3，存储CS选择子，在RPL为3
    proc_stack->cs  = SELECTOR_U_CODE;

    // 关键点5、6：使得栈中eflags的IF位为1，IOPL位位0
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);

    // 关键点4： 为用户进程分配3特权级下的栈，并且将栈中段寄存器的选择之必须指向DPL为3的内存段
    proc_stack->esp = (void *)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE);
    proc_stack->ss  = SELECTOR_U_DATA;

    // 通过内联汇编，将esp替换曾proc_stack，然后通过jmp intr_exit使得程序条大中断出口地址intr_exit，然后将其
    // 载入CPU的寄存器，从而使得假装退出中断
    // 关键点1： 从中断返回，必须经过intr_exit，即使是假装
    asm volatile("movl %0, %%esp; jmp intr_exit"
                 :
                 : "g"(proc_stack)
                 : "memory");

    return ;
}

// 激活页表
void page_dir_activate(struct task_struct *p_thread)
{
    /**
     * @brief page_dir_activate
     * 执行此函数时,当前任务可能是线程。之所以对线程也要重新安装页表, 原因是上一次被调度的可能是进程,
     * 否则不恢复页表的话,线程就会使用进程的页表了。
     * 
     */

    // 若为内核线程,需要重新填充页表为0x100000
    uint32_t pagedir_phy_addr = 0x100000;     // 默认为内核的页目录物理地址,也就是内核线程所用的页目录表

    if (p_thread->pgdir != NULL) // 用户态进程有自己的页目录表
    { 
        // 将虚拟地址p_thread->pgdir转为物理地址
        pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
    }

    // 更新页目录寄存器cr3,使新页表生效
    asm volatile("movl %0, %%cr3"
                 :
                 : "r"(pagedir_phy_addr)
                 : "memory");

    return;
}

// 激活线程或进程的页表,更新tss中的esp0为进程的特权级0的栈
void process_activate(struct task_struct *p_thread)
{
    ASSERT(p_thread != NULL);

    // 激活该进程或线程的页表
    page_dir_activate(p_thread);

    // 内核线程特权级本身就是0,处理器进入中断时并不会从tss中获取0特权级栈地址,故不需要更新esp0
    if (p_thread->pgdir)
    {

        // 更新该进程的esp0,用于此进程被中断时保留上下文
        update_tss_esp(p_thread);
    }

    return;
}

// 创建页目录表,将当前页表的表示内核空间的pde复制,成功则返回页目录的虚拟地址,否则返回-1
uint32_t *create_page_dir(void)
{

    // 用户进程的页表不能让用户直接访问到,所以在内核空间来申请
    uint32_t *page_dir_vaddr = get_kernel_pages(1);
    if (page_dir_vaddr == NULL)
    {
        console_put_str("create_page_dir: get_kernel_page failed!");
        return NULL;
    }

    // 1 先复制页表
    // page_dir_vaddr + 0x300*4 是内核页目录的第768项
    memcpy((uint32_t *)((uint32_t)page_dir_vaddr + 0x300 * 4), (uint32_t *)(0xfffff000 + 0x300 * 4), 1024);

    // 2 更新页目录地址
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);

    // 页目录地址是存入在页目录的最后一项,更新页目录地址为新页目录的物理地址
    page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;

    return page_dir_vaddr;

    /**
     * @brief page_dir_vaddr + 0x300*4 是内核页目录的第768项的解释
     * 
     *                                           .------------.       .------------.
     *                                           |            |       |            |           
     *                                           |            |-----> |            | 
     *                                           |            |       |            |  
     *                                     .---> |            |       |kernel Pages|                                     
     *                                     |     |            |       |            |
     *  .---------------------------. 0    |     |            |       |   Tables   |
     *  |                           |      |     |            |       |            |
     *  |      3GB  user space      | -----'     |            |       '------------'                   
     *  |                           |            |            |
     *  '---------------------------' 767        |  Physical  |
     *  .---------------------------. 768        |   memory   |
     *  |      1GB kernel space     | -----.     |            |
     *  '---------------------------' 1023 |     |            |      
     *                                     |     |            |
     *                                     |     |            |    
     *                                     '---> |            |
     *                                           |            |                         
     *                                           |            |
     *                                           |            | 
     *                                           |            |       .------------. 
     *                                           |            |       |            | 
     *                                           |            |-----> |            | 
     *                                           |            |       |            |  
     *                                           '------------'       |kernel Pages|
     *                                                                |            |
     *                                                                |   Tables   |
     *                                                                |            | 
     *                                                                '------------' 
     */
}

// 创建用户进程虚拟地址位图,创建虚拟内存池
void create_user_vaddr_bitmap(struct task_struct *user_prog)
{
    /**
     * @brief user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START
     * 
     * 是位图中所管理的内存空间的起始地址,为用户进程定的起始地址是USER_VADDR_START
     * 该值定义在process.h,其值为0x8048_000
     * 
     */

    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;

    // 用来记录位图所需的内存页数，DIV_ROUND_UP定义在global.h中进行取整
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);

    // 为位图分配内存
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);

    // 记录位图长度
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;

    // 进行位图初始化，用户虚拟地址位图创建完成
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);

    return;
}

// 创建用户进程
void process_execute(void *filename, char *name)
{
    // pcb内核的数据结构,由内核来维护进程信息,因此要在内核内存池中申请
    // 1. 申请1页内存创建进程的PCB
    struct task_struct *thread = get_kernel_pages(1);

    // 2. 对thread进行初始化
    init_thread(thread, name, default_prio);

    // 3. 为用户进程创建管理虚拟地址空间
    create_user_vaddr_bitmap(thread);

    // 4. 创建线程
    thread_create(thread, start_process, filename);

    // 5. 为进程创建页表
    thread->pgdir = create_page_dir();
    block_desc_init(thread->u_block_desc); // 创建内存块，在malloc中处理

    // 6. 加入队列并初始化状态
    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    list_append(&thread_ready_list, &thread->general_tag);

    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    list_append(&thread_all_list, &thread->all_list_tag);
    intr_set_status(old_status);

    return;
}