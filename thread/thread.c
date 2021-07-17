#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "memory.h"
#include "../userprog/process.h"
#include "../thread/sync.h"
#include "../fs/file.h"
#include "../fs/fs.h"
#include "../lib/stdio.h"


// pid的位图,最大支持1024个pid
uint8_t pid_bitmap_bits[128] = {0};

// pid池
struct pid_pool
{
    struct bitmap pid_bitmap;     // pid位图
    uint32_t pid_start;           // 起始pid
    struct lock pid_lock;         // 分配pid锁
} pid_pool;

struct task_struct *main_thread;            // 主线程PCB
struct task_struct *idle_thread;            // idle线
struct list        thread_ready_list;       // 就绪队列,以后每创建一个线程就将其加道此队列中
struct list        thread_all_list;         // 所有线程队列
struct lock        pid_lock;                // 分配pid锁
static struct list_elem *thread_tag;        // 用于保存队列中的线程结点

extern void switch_to(struct task_struct *cur, struct task_struct *next);
extern void init(void);


// 系统空闲时运行的线程
static void idle(void *arg UNUSED)
{
    while (1)
    {
        thread_block(TASK_BLOCKED);

        //执行hlt时必须要保证目前处在开中断的情况下
        asm volatile("sti; hlt"
                     :
                     :
                     : "memory");

    }

    return;
}

// 获取当前线程pcb指针
struct task_struct *running_thread(void)
{
    // 栈指针
    uint32_t esp;

    // 取esp整数部分，即pcb起始地址
    asm("mov %%esp, %0"
        : "=g"(esp)); 

    return (struct task_struct *)(esp & 0xfffff000);
}

// 由kernel_thread去执行function(func_arg)
static void kernel_thread(thread_func *function, void *func_arg)
{
    /**
     * @brief kernel_thread
     * 
     * 线程的首次运行是由时钟中断处理函数调用任务调度器schedule完成的，进入中断后处理器会自动
     * 关中断，因此在执行function前也要打开中断，否则kernel_thread中的function在关中断的情况下运行
     * ，也就是时钟中断被屏蔽了，再也不会调度到新的线程，function会处理处理器
     * 
     */

    intr_enable();         // 执行function前要开中断,避免后面的时钟中断被屏蔽,而无法调度其它线程
    
    function(func_arg);

    return;
}

// 初始化pid池
static void pid_pool_init(void)
{
    pid_pool.pid_start                 = 1;
    pid_pool.pid_bitmap.bits           = pid_bitmap_bits;
    pid_pool.pid_bitmap.btmp_bytes_len = 128;

    bitmap_init(&pid_pool.pid_bitmap);
    lock_init(&pid_pool.pid_lock);

    return;
}

// 分配pid
static pid_t allocate_pid(void)
{
    lock_acquire(&pid_pool.pid_lock);
    int32_t bit_idx = bitmap_scan(&pid_pool.pid_bitmap, 1);

    bitmap_set(&pid_pool.pid_bitmap, bit_idx, 1);

    lock_release(&pid_pool.pid_lock);

    return (bit_idx + pid_pool.pid_start);
}

// 释放pid
void release_pid(pid_t pid)
{
    lock_acquire(&pid_pool.pid_lock);
    int32_t bit_idx = pid - pid_pool.pid_start;

    bitmap_set(&pid_pool.pid_bitmap, bit_idx, 0);
    lock_release(&pid_pool.pid_lock);

    return;
}

// fork进程时为其分配pid,因为allocate_pid已经是静态的,
// 别的文件无法调用.不想改变函数定义了,故定义fork_pid函数来封装一下。
pid_t fork_pid(void)
{
    return allocate_pid();
}

// 初始化线程栈thread_stack,将待执行的函数和参数放到thread_stack中相应的位置
void thread_create(struct task_struct *pthread, thread_func function, void *func_arg)
{
    /**
     * @brief pthread->self_kstack -= sizeof(struct intr_stack); 
     * 
     * 1. 线程进入中断后，位于kernel.S的中断代码会通过此栈来保存上下文
     * 2. 将来实现用户进程时，会将用户进程的初始信息放在中断栈
     * 
     * 减去中断栈的大小之后，pthread->self_kstack指向PCB种的中断栈下面的地址
     * 
     * pthread->self_kstack -= sizeof(struct thread_stack); 也是同理
     */

    // 先预留中断使用栈的空间,可见thread.h中定义的结构
    pthread->self_kstack -= sizeof(struct intr_stack);

    // 再留出线程栈空间,可见thread.h中定义
    pthread->self_kstack -= sizeof(struct thread_stack); 

    struct thread_stack *kthread_stack = (struct thread_stack *)pthread->self_kstack;

    kthread_stack->eip      = kernel_thread;
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;

    // 将那4个寄存器初始化
    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;

    return;
}

// 初始化线程基本信息
void init_thread(struct task_struct *pthread, char *name, int prio)
{
    memset(pthread, 0, sizeof(*pthread));     // 先清0一页
    pthread->pid = allocate_pid();            // 分配pid
    strcpy(pthread->name, name);              // 将线程名写入PCB中的name数组

    if (pthread == main_thread)
    {
        // 由于是main函数也封装成一个线程，并且它一直运行，因此直接设置为TASK_RUNNING
        pthread->status = TASK_RUNNING;       // 运行状态
    }
    else
    {
        // 否则先设置为就绪态
        pthread->status = TASK_READY;         // 就绪态
    }

    // self_kstack是线程自己在内核态下使用的栈顶地址，被初始化为线程PCB的最顶端
    pthread->self_kstack   = (uint32_t *)((uint32_t)pthread + PG_SIZE);
    pthread->priority      = prio;            // 优先级
    pthread->ticks         = prio;            // 时间即为优先级
    pthread->elapsed_ticks = 0;               // 执行的时间数
    pthread->pgdir         = NULL;            // 所分配的页数

    // 预留标准输入输出
    pthread->fd_table[0]   = 0;               // 标准输入
    pthread->fd_table[1]   = 1;               // 标准输出
    pthread->fd_table[2]   = 2;               // 标准错误

    // 其余的全置为-1
    uint8_t fd_idx = 3;

    while (fd_idx < MAX_FILES_OPEN_PER_PROC)
    {
        pthread->fd_table[fd_idx] = -1;
        fd_idx++;
    }

    // 小彩蛋，我的生日
    pthread->stack_magic   = 0x20000720;      // 自定义的魔数,没用的
    pthread->cwd_inode_nr  = 0;               // 以根目录做为默认工作路
    pthread->parent_pid    = -1;              // -1表示没有父进

    return;
}

// 创建一优先级为prio的线程,线程名为name,线程所执行的函数是function(func_arg)
struct task_struct *thread_start(char *name, int prio, thread_func function, void *func_arg)
{
    // pcb都位于内核空间,包括用户进程的pcb也是在内核空间
    struct task_struct *thread = get_kernel_pages(1);       // 先申请一页内存

    init_thread(thread, name, prio);                        // 初始化刚刚建立的thread线程
    thread_create(thread, function, func_arg);              // 创建刚刚建立的进程

    // 确保之前不在就绪队列中
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    list_append(&thread_ready_list, &thread->general_tag);  // 加入就绪线程队列

    // 确保之前不在全部队列中
    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    list_append(&thread_all_list, &thread->all_list_tag);   // 加入全部线程队列

    return thread;
}

// 将kernel中的main函数完善为主线程，为主线程的PCB写入主线程
static void make_main_thread(void)
{
    /**
     * @brief 
     * 因为main线程早已运行,咱们在loader.S中进入内核时的mov esp,0xc009f000,
     * 就是为其预留了tcb,地址为0xc009e000,因此不需要通过get_kernel_page另分配一页
     * 
     */

    main_thread = running_thread();
    init_thread(main_thread, "main", 31);

    // main函数是当前线程,当前线程不在thread_ready_list中,所以只将其加在thread_all_list中.
    ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
    list_append(&thread_all_list, &main_thread->all_list_tag);

    return ;
}

// 实现RR轮询调度，Round-Robin Scheduling,实现完整调度过程的第三步
//  实现任务调度
void schedule(void)
{
    /**
     * @brief 
     * 将当前线程换下处理器，并在就绪队列中找出下个可运行的程序，将其换上处理器.
     * schedule主要内容就是读写就绪队列，因此它不需要参数。
     * 
     */

    ASSERT(intr_get_status() == INTR_OFF);

    struct task_struct *cur = running_thread();     // 获取当前运行线程的PCB，将其存入PCB指针cur中

    if (cur->status == TASK_RUNNING)                // 若此线程只是cpu时间片到了,将其加入到就绪队列尾
    {
        ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
        list_append(&thread_ready_list, &cur->general_tag);
        cur->ticks = cur->priority;                 // 重新将当前线程的ticks再重置为其priority;
        cur->status = TASK_READY;
    }
    else
    {
        /**
         * @brief 
         * 若此线程需要某事件发生后才能继续上cpu运行,不需要将其加入队列,因为当前线程不在就绪队列中。
         * 
         */
    }

    if (list_empty(&thread_ready_list))
    {
        thread_unblock(idle_thread);
    }

    ASSERT(!list_empty(&thread_ready_list));
    thread_tag = NULL;                              // thread_tag清空

    // 将thread_ready_list队列中的第一个就绪线程弹出,准备将其调度上cpu.
    thread_tag = list_pop(&thread_ready_list);

    // 这里要进行类型与地址转换，将其结点转为PCB来使用
    struct task_struct *next = elem2entry(struct task_struct, general_tag, thread_tag);

    /**
     * @brief elem2entry and offset in /lib/kernel/list.h
     * 
     * #define offset(struct_type,member) (int)(&((struct_type*)0)->member)
     * #define elem2entry(struct_type, struct_member_name, elem_ptr) \
     * (struct_type*)((int)elem_ptr - offset(struct_type, struct_member_name))
     * 
     * elem_ptr要转换的地址，struct_member_name是elem_ptr所在结构体中对应的地址的成员名字
     * struct_type是elem_ptr所属的结构体
     * elem2entry的作用是将指针elem_ptr转化为struct_type类型的指针
     * 
     */

    /**
     * @brief elem2entry(struct task_struct, general_tag, thread_tag);
     * 访问结构体成员俄两种方法是"结构体变量.成员"和"结构体指针变量->成员"
     * 他们的访问原理是“结构体变量的地址+成员的偏移量
     * 宏elem2entry的转换原理
     * 1. 用结构体成员的地址减去成员在结构体中的偏移量，先获取到结构体起始地址。
     * 2. 再通过强制类型转换将第1步中的地址转换成结构体类型。
     */

    next->status = TASK_RUNNING;

    // 激活任务页表
    process_activate(next);
    switch_to(cur, next);

    return ;
}

// 当前线程将自己阻塞,标志其状态为stat
void thread_block(enum task_status stat)
{
    // stat取值为TASK_BLOCKED,TASK_WAITING,TASK_HANGING,也就是只有这三种状态才不会被调度
    ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || (stat == TASK_HANGING)));

    enum intr_status old_status    = intr_disable();       // 关中断
    struct task_struct *cur_thread = running_thread();     // 获得当前运行线程的状态

    cur_thread->status = stat;                             // 置其状态为stat

    schedule();                                            // 将当前线程换下处理器

    // 待当前线程被解除阻塞后才继续运行下面的intr_set_status
    intr_set_status(old_status);

    return;
}

// 将线程pthread解除阻塞
void thread_unblock(struct task_struct *pthread)
{
    enum intr_status old_status = intr_disable();

    // 判断当前线程的状态
    ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) \
         || (pthread->status == TASK_HANGING)));

    if (pthread->status != TASK_READY)
    {

        ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));

        if (elem_find(&thread_ready_list, &pthread->general_tag))
        {
            PANIC("thread_unblock:  blocked thread in ready_list\n");
        }

        list_push(&thread_ready_list, &pthread->general_tag); // 放到队列的最前面,使其尽快得到调度
        pthread->status = TASK_READY;                         // 喂，起床了别睡了
    }

    intr_set_status(old_status); // 设置状态

    return;
}

// 主动让出cpu,换其它线程运行
void thread_yield(void)
{
    // 1. 把当前任务重新加入到就绪队列
    struct task_struct *cur = running_thread();
    enum intr_status old_status = intr_disable();

    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));

    list_append(&thread_ready_list, &cur->general_tag);

    // 2. 把当前任务的status置为TASK_READY
    cur->status = TASK_READY;

    // 3. 最后重新调度新的任务
    schedule();

    return;
}

// 以填充空格的方式输出buf
static void pad_print(char *buf, int32_t buf_len, void *ptr, char format)
{
    memset(buf, 0, buf_len);
    uint8_t out_pad_0idx = 0;

    switch (format)
    {
    case 's':
        out_pad_0idx = sprintf(buf, "%s", ptr);
        break;

    case 'd':
        out_pad_0idx = sprintf(buf, "%d", *((int16_t *)ptr));

    case 'x':
        out_pad_0idx = sprintf(buf, "%x", *((uint32_t *)ptr));
    }

    while (out_pad_0idx < buf_len) // 以空格填充
    {
        buf[out_pad_0idx] = ' ';
        out_pad_0idx++;
    }

    sys_write(stdout_no, buf, buf_len - 1);

    return;
}

// 用于在list_traversal函数中的回调函数,用于针对线程队列的处理
static bool elem2thread_info(struct list_elem *pelem, int arg UNUSED)
{
    struct task_struct *pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    char out_pad[16] = {0};

    pad_print(out_pad, 16, &pthread->pid, 'd');

    if (pthread->parent_pid == -1)
    {
        pad_print(out_pad, 16, "NULL", 's');
    }
    else
    {
        pad_print(out_pad, 16, &pthread->parent_pid, 'd');
    }

    switch (pthread->status)
    {
    case 0:
        pad_print(out_pad, 16, "RUNNING", 's');
        break;

    case 1:
        pad_print(out_pad, 16, "READY", 's');
        break;

    case 2:
        pad_print(out_pad, 16, "BLOCKED", 's');
        break;

    case 3:
        pad_print(out_pad, 16, "WAITING", 's');
        break;

    case 4:
        pad_print(out_pad, 16, "HANGING", 's');
        break;

    case 5:
        pad_print(out_pad, 16, "DIED", 's');
    }

    pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');

    memset(out_pad, 0, 16);
    ASSERT(strlen(pthread->name) < 17);

    memcpy(out_pad, pthread->name, strlen(pthread->name));
    strcat(out_pad, "\n");

    sys_write(stdout_no, out_pad, strlen(out_pad));

    // 此处返回false是为了迎合主调函数list_traversal,只有回调函数返回false时才会继续调用此函数
    return false;
}

// 打印任务列表
void sys_ps(void)
{
    char *ps_title = "PID            PPID           STAT           TICKS          COMMAND\n";
    sys_write(stdout_no, ps_title, strlen(ps_title));
    list_traversal(&thread_all_list, elem2thread_info, 0);

    return;
}

// 回收thread_over的pcb和页表,并将其从调度队列中去除
void thread_exit(struct task_struct *thread_over, bool need_schedule)
{
    // 要保证schedule在关中断情况下调用
    intr_disable();
    thread_over->status = TASK_DIED;

    // 如果thread_over不是当前线程,就有可能还在就绪队列中,将其从中删除
    if (elem_find(&thread_ready_list, &thread_over->general_tag))
    {
        list_remove(&thread_over->general_tag);
    }

    if (thread_over->pgdir) // 如是进程,回收进程的页表
    {
        mfree_page(PF_KERNEL, thread_over->pgdir, 1);
    }

    // 从all_thread_list中去掉此任务
    list_remove(&thread_over->all_list_tag);

    // 回收pcb所在的页,主线程的pcb不在堆中,跨过
    if (thread_over != main_thread)
    {
        mfree_page(PF_KERNEL, thread_over, 1);
    }

    // 归还pid
    release_pid(thread_over->pid);

    // 如果需要下一轮调度则主动调用schedule
    if (need_schedule)
    {
        schedule();
        PANIC("thread_exit: should not be here\n");
    }

    return;
}

// 比对任务的pid
static bool pid_check(struct list_elem *pelem, int32_t pid)
{
    struct task_struct *pthread = elem2entry(struct task_struct, all_list_tag, pelem);

    if (pthread->pid == pid)
    {
        return true;
    }

    return false;
}

// 根据pid找pcb,若找到则返回该pcb,否则返回NULL
struct task_struct *pid2thread(int32_t pid)
{
    struct list_elem *pelem = list_traversal(&thread_all_list, pid_check, pid);

    if (pelem == NULL)
    {
        return NULL;
    }

    struct task_struct *thread = elem2entry(struct task_struct, all_list_tag, pelem);
    return thread;
}

// 初始化线程环境
void thread_init(void)
{
    put_str("\nthread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    lock_init(&pid_lock);
    pid_pool_init();
    process_execute(init, "init");

    // 将当前main函数创建为线程
    make_main_thread();

    // 创建idle线程
    idle_thread = thread_start("idle", 10, idle, NULL);

    put_str("thread_init done\n");

    return;
}