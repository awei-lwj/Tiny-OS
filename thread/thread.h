#ifndef __THREAD__THREAD__H
#define __THREAD__THREAD__H
#include "stdint.h"
#include "../lib/kernel/list.h"
#include "../lib/kernel/bitmap.h"
#include "../kernel/memory.h"

#define TASK_NAME_LEN 16
#define MAX_FILES_OPEN_PER_PROC 8

// 自定义通用函数类型,它将在很多线程函数中做为形参类型
typedef void thread_func(void *);
typedef int16_t pid_t;

// 进程或线程的状态
enum task_status
{
    // 具体切换流传看我的时序图
    TASK_WAITING,     // 等待态, 进程在调度的时候因为缺少I/O设备等资源而陷入的状态
    TASK_READY,       // 就绪态, 什么都有了就缺CPU一旦获取CPU资源就可以进入运行态
    TASK_RUNNING,     // 运行态
    TASK_DIED,        // 终止态
    TASK_BLOCKED,     // 阻塞态
    TASK_HANGING,     // 挂起态，暂时放在外存中进行处理
};

/**
 * @brief intr_stack
 * 
 * 此结构用于中断发生时保护程序(线程或进程)的上下文环境:
 * 进程或线程被外部中断或软中断打断时,会按照此结构压入上下文寄存器,  intr_exit中的出栈操作是此结构的逆操作
 * 此栈在线程自己的内核栈中位置固定,所在页的最顶端
 * 
 * 初始情况下此栈在线程自己的内核技中位置固定，在PCB所在页的最顶端，每次进入中断时就不一定了，
 * 如果进入中断时不涉及到特权级变化，它的位置就会在当前的es 之下，否则处理器会从TSS中获得新的
 * esp 的值，然后该枝在新的 esp 之下
 * 
 */

// 中断栈，位于线程的内核栈中，位于PCB的高地址处
struct intr_stack
{

    uint32_t vec_no;       // kernel.S 宏VECTOR中push %1压入的中断号
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;    // 虽然pushad把esp也压入,但esp是不断变化的,所以会被popad忽略
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    // 以下由cpu从低特权级进入高特权级时压入
    uint32_t err_code;     // err_code会被压入在eip之后
    void (*eip)(void);
    uint32_t cs;
    uint32_t eflags;
    void *esp;
    uint32_t ss;
};

/**
 * @brief thread_stack 
 * 
 * 线程自己的栈,用于存储线程中待执行的函数，此结构在线程自己的内核栈中位置不固定,
 * 用在switch_to时保存线程环境，实际位置取决于实际运行情况。
 * 
 * 他有两个作用：
 * 1. 是在线程首次运行时，线程栈用于存储线程所需的相关数据
 * 2. 为主调函数保护那四个寄存器
 * 
 */

// 线程栈，位于线程的内核栈中，位于PCB的高地址处
struct thread_stack
{

    // 用switch_to切换的时候，先在线程栈压入这4个寄存器的值
    uint32_t ebp;              // 被调用的函数必须为主调函数保存好这5个寄存器的值
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
//  uint32_t esp               // esp保护部分不归他管得，他是调用约定来保证的

    // 线程第一次执行时,eip指向待调用的函数kernel_thread,其它时候,eip是指向switch_to的返回地址

    void (*eip)(thread_func *func, void *func_arg);  // eip便是保存的待运行的函数的地址

    //   以下仅供第一次被调度上cpu时使用
//-----------------------------------------------------------------------------------------------------
    void(*unused_retaddr);     // 参数unused_ret为返回地址，在返回的栈帧占个位置
    thread_func *function;     // 由Kernel_thread所调用的函数名
    void *func_arg;            // 由Kernel_thread所调用的函数所需的参数
};

// 进程或线程的pcb,程序控制块
struct task_struct
{

    uint32_t         *self_kstack;        // 各内核线程都用自己的内核栈
    pid_t            pid;                 // 进程号
    enum task_status status;              // 线程的状态
    uint8_t          priority;            // 线程优先级
    char             name[TASK_NAME_LEN]; // 记录任务的名字，长度为16

    // 每次在处理器上执行的时间嘀嗒数，每次任务被调度上处理器后执行的时间片越长，优先级越高
    uint8_t          ticks;                
    uint32_t         elapsed_ticks;       // 此任务执行了多久，从开始执行，到运行结束后所经历的总时钟数
    int32_t          fd_table[MAX_FILES_OPEN_PER_PROC];     // 文件描述符数组

    struct list_elem general_tag;        // general_tag的作用是用于线程在一般的队列中的结点，线程的标签
    struct list_elem all_list_tag;       // all_list_tag的作用是用于线程队列thread_all_list中的结点

    uint32_t         *pgdir;             // 进程自己页表的虚拟地址空间，而线程没有

    struct virtual_addr userprog_vaddr;  // 进程自己的地址空间

    // 用户进程内存块描述符，实现堆管理
    struct mem_block_desc u_block_desc[DESC_CNT];

    uint32_t         cwd_inode_nr;       // 进程所在的工作目录的inode编号
    int16_t          parent_pid;         // 父进程pid
    int8_t           exit_status;        // 进程结束时自己调用exit传入的参数
    uint32_t         stack_magic;        // 用这串数字做栈的边界标记,用于检测栈的溢出
};

extern struct list thread_ready_list;    // 就绪队列
extern struct list thread_all_list;      // 全部队列



// 实现任务调度
void schedule(void); // RR轮询调度，Round-Robin Scheduling

// 初始化线程环境
void thread_init(void);

// 初始化线程栈thread_stack,将待执行的函数和参数放到thread_stack中相应的位置
void thread_create(struct task_struct *pthread, thread_func function, void *func_arg);

// 当前线程将自己阻塞,标志其状态为stat
void thread_block(enum task_status stat);

// 将线程pthread解除阻塞
void thread_unblock(struct task_struct *pthread);

// 初始化线程基本信息
void init_thread(struct task_struct *pthread, char *name, int prio);

// 创建一优先级为prio的线程,线程名为name,线程所执行的函数是function(func_arg)
struct task_struct *thread_start(char *name, int prio, thread_func function, void *func_arg);

// 获取当前线程pcb指针
struct task_struct *running_thread(void);

// 主动让出cpu,换其它线程运行
void thread_yield(void);

// fork进程时为其分配pid
pid_t fork_pid(void);

// 打印任务列表
void sys_ps(void);

// 回收thread_over的pcb和页表,并将其从调度队列中去除
void thread_exit(struct task_struct *thread_over, bool need_schedule);

// 根据pid找pcb,若找到则返回该pcb,否则返回NULL
struct task_struct *pid2thread(int32_t pid);

// 释放pid
void release_pid(pid_t pid);



#endif // __THREAD__THREAD__H