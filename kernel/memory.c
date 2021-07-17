#include "../kernel/memory.h"
#include "../kernel/global.h"
#include "../kernel/debug.h"
#include "../lib/stdint.h"
#include "../lib/string.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/bitmap.h"
#include "../thread/sync.h"             // 保证进程空间的互斥性
#include "interrupt.h"
  

#define MEM_BITMAP_BASE 0xc009a000      // 位图地址

/**
 * 
 * #define  PG_SIZE 4096
 * #define  K_HEAP_START 0x009a000  中0x009a00
 * 
 * 这两个东西的解释
 * 因为0xc009f000是内核主线程栈顶，0xc009e000是内核主线程的pcb,一个页框大小的位图可表示128M内存, 位图位置安排在地址0xc009a000,
 * 这样本系统最大支持4个页框的位图,即512M
 * 
 * 说白了就是为main预留了PCB空间
 * 
 * 内核预计在70KB左右，转载到0xc009f000以下是绰绰有余的，所以主线程的栈地址0xc009f000是我们在低端1MB中所用的最高地址
 * 
 * 当前虚拟机配置了32MB的物理内存，这32MB物理内存需要1024字节的位图，也就是仅占四分之1页，
 * 1页大小的位图可管理 128MB的内存 对于目前的 32MB 内存来说，用一页内存来存储位图还浪费呢，
 * 不过为了扩展，在此打算支持4页内存的位图，即最大可管理512MB的物理内存
 * 
 * 0xc009e000 - 0x1000*4 = 0xc009a000
 */

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)     // 用于返回虚拟地址的高10位，即pde索引部分
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)     // 用于返回虚拟地址的中间10位，即pte索引部分

// 内核所使用的堆空间起始虚拟地址
// 0xc0000000是内核从虚拟地址3G起. 0x100000意指跨过低端1M内存,使虚拟地址在逻辑上连续
#define K_HEAP_START 0xc0100000

// 内存池结构,生成两个实例用于管理内核内存池和用户内存池
struct pool
{
    struct bitmap pool_bitmap;     // 本内存池用到的位图结构,用于管理物理内存

    uint32_t phy_addr_start;       // 本内存池所管理物理内存的起始地址
    uint32_t pool_size;            // 本内存池字节容量

    struct lock lock;              // 申请内存的时候保持互斥
};

// 内存仓库arena元信息
struct arena
{
    /**
     * @brief 
     * arge为true时,cnt表示的是页框数。 否则cnt表示空闲mem_block数量
     */

    // 此arena关联的mem_block_desc
    struct mem_block_desc *desc; 

    uint32_t cnt;
    bool     large;
};

struct mem_block_desc k_block_descs[DESC_CNT];     // 内核内存块描述符数
struct pool kernel_pool, user_pool;                // 生成内核内存池和用户内存池
struct virtual_addr kernel_vaddr;                  // 此结构是用来给内核分配虚拟地址

// 在pf表示的虚拟内存池中申请pg_cnt个虚拟页,成功则返回虚拟页的起始地址, 失败则返回NULL
static void *vaddr_get(enum pool_flags pf, uint32_t pg_cnt)
{
    // 用于存储分配的起始虚拟地址
    int vaddr_start   = 0;   
    // 用于存储位图扫描函数bitmap_scan的返回值，默认值位-1
    int bit_idx_start = -1;
    
    uint32_t cnt = 0;

    if (pf == PF_KERNEL) // 如果其值等于PF_KERNEL,便认为是在内核虚拟地址池申请地址
    {
        // 将bit_idx_start转换为虚拟地址
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt); 

        if (bit_idx_start == -1)
        {
            return NULL;
        }

        while (cnt < pg_cnt)
        {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }

        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    }
    else // 用户态进程内存池
    { 

        struct task_struct *cur = running_thread();
        bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt);

        if (bit_idx_start == -1)
        {
            return NULL;
        }

        while (cnt < pg_cnt)
        {
            bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }

        vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;

        // (0xc0000000 - PG_SIZE)做为用户3级栈已经在start_process被分配
        ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));

    } // end if

    return (void *)vaddr_start;
}

// 得到虚拟地址vaddr对应的pte指针
uint32_t *pte_ptr(uint32_t vaddr)
{
    /**
    * @brief 
    * 1 .先访问到页表自己 
    * 2. 再用页目录项pde(页目录内页表的索引)做为pte的索引访问到页表
    * 3. 再用pte的索引做为页内偏移
    * 
    */

    //                          1.             2.                            3.
    uint32_t *pte = (uint32_t *)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);

    /** 解释 (0xffc00000 + ((vaddr & 0xffc00000) >> 10) +  PTE_IDX(vaddr) * 4)
     * 
     * 1. 高10位用于定位页目录项，最后一页目录项存储的正是页目录表物理地址，需要让地址的高10位指向最后一个页目录项，即1023个pde
     *    1023 = 0x3ff -> 10位后 = 0xffc00000,于是此处让处理器自动从1023的pde取出页目录项的地址，此处地址为0x100000
     *    PAGE_DIR_TABLE_POS   equ  0x1000_00
     *
     * 2. vaddr & 0xffc00000 将vaddr的高10位提取出来， >> 10 作为新地址new_addr的中间10位
     *
     * 3. 最后vaddr 的中间 IO 位转换成处理器眼里的 12 位长度的页内偏移量就行了
     *  
     */

    return pte;
}

// 得到虚拟地址vaddr对应的pde的指针
uint32_t *pde_ptr(uint32_t vaddr)
{
    // 0xfffff是用来访问到页表本身所在的地址
    uint32_t *pde = (uint32_t *)((0xfffff000) + PDE_IDX(vaddr) * 4);
    
    return pde;
}

// 在m_pool指向的物理内存池中分配1个物理页,成功则返回页框的物理地址,失败则返回NULL
static void *palloc(struct pool *m_pool)
{
    // 扫描或设置位图要保证原子操作
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);     // 找一个物理页面

    if (bit_idx == -1)
    {
        return NULL;
    }

    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);          // 将此位bit_idx置1
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);

    return (void *)page_phyaddr;
}

// 页表中添加虚拟地址_vaddr与物理地址_page_phyaddr的映射
static void page_table_add(void *_vaddr, void *_page_phyaddr)
{
    uint32_t vaddr        = (uint32_t)_vaddr;
    uint32_t page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t *pde         = pde_ptr(vaddr);      // 获取虚拟地址addr所在的pde的的虚拟地址

    // 虚拟地址和物理地址的映射关系是在页表中完成的，本质上在页表中添加次虚拟地址对应的页表项pte
    // 并将物理页的物理地址写入此页表项pte中
    uint32_t *pte = pte_ptr(vaddr);

    /** 
     * @brief
     * 执行*pte,会访问到空的pde。所以确保pde创建完成后才能执行*pte,
     * 否则会引发page_fault。因此在*pde为0时,*pte只能出现在下面else语句块中的*pde后面。
     * 
     */

    // 先在页目录内判断目录项的P位，若为1,则表示该表已存在
    if (*pde & 0x00000001)
    {
        // 页目录项和页表项的第0位为P,此处判断目录项是否存在
        ASSERT(!(*pte & 0x00000001));

        // 只要是创建页表,pte就应该不存在
        if (!(*pte & 0x00000001))
        {
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1); // US=1,RW=1,P=1
        }
        else
        {

            //应该不会执行到这，因为上面的ASSERT会先执行。
            PANIC("pte repeat");
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1); // US=1,RW=1,P=1
        }

    }
    else
    {   // 页目录项不存在,所以要先创建页目录再创建页表项.

        // 页表中用到的页框一律从内核空间分配
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);

        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);

        /**
         * @brief 
         * 
         * 分配到的物理页地址pde_phyaddr对应的物理内存清0, 避免里面的陈旧数据变成了页表项,从而让页表混乱.
         * 访问到pde对应的物理地址,用pte取高20位便可, 因为pte是基于该pde对应的物理地址内再寻址,
         * 把低12位置0便是该pde对应的物理页的起始
         * 
         */

        // 对刚刚申请的物理页初始化为0
        memset((void *)((int)pte & 0xfffff000), 0, PG_SIZE);

        ASSERT(!(*pte & 0x00000001));

        // 为vaddr所在的pte进行赋值
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1); // US=1,RW=1,P=1

    } // end if

    return ;
}

// 分配pg_cnt个页空间,成功则返回起始虚拟地址,失败时返回NULL
void *malloc_page(enum pool_flags pf, uint32_t pg_cnt)
{
    // 判断申请的内存页数pg_cnt是否超出年物理内存池的容量
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);

    /**
     * @brief 
     * 
     * malloc_page的原理是三个动作的合成: 
     * 1. 通过vaddr_get在虚拟内存池中申请虚拟地址
     * 2. 通过palloc在物理内存池中申请物理页
     * 3. 通过page_table_add将以上得到的虚拟地址和物理地址在页表中完成映射
     * 
     */

    // 1. 通过vaddr_get在虚拟内存池中申请虚拟地址
    void *vaddr_start = vaddr_get(pf, pg_cnt);
    if (vaddr_start == NULL)
    {
        return NULL;
    }

    uint32_t vaddr        = (uint32_t)vaddr_start, cnt = pg_cnt;
    struct pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

    // 因为虚拟地址是连续的,但物理地址可以是不连续的,所以逐个做映射
    while (cnt-- > 0)
    {
        // 2. 通过palloc在物理内存池中申请物理页
        void *page_phyaddr = palloc(mem_pool);

        // 失败时要将曾经已申请的虚拟地址和物理页全部回滚，在将来完成内存回收时再补充
        if (page_phyaddr == NULL)
        {
            return NULL;
        }

        // 3. 通过page_table_add将以上得到的虚拟地址和物理地址在页表中完成映射
        // 在页表中做映射
        page_table_add((void *)vaddr, page_phyaddr); 
        vaddr += PG_SIZE;                            // 下一个虚拟页

    }

    return vaddr_start;
}

// 从内核物理内存池中申请pg_cnt页内存,成功则返回其虚拟地址,失败则返回NULL
void *get_kernel_pages(uint32_t pg_cnt)
{
    lock_acquire(&kernel_pool.lock);
    void *vaddr = malloc_page(PF_KERNEL, pg_cnt);

    if (vaddr != NULL)
    {
        // 若分配的地址不为空,将页框清0后返回
        memset(vaddr, 0, pg_cnt * PG_SIZE);

    }

    lock_release(&kernel_pool.lock);

    return vaddr;
}

// 在用户空间中申请4k内存，并返回虚地址
void *get_user_pages(uint32_t pg_cnt)
{
    lock_acquire(&user_pool.lock);

    void *vaddr = malloc_page(PF_USER, pg_cnt);
    memset(vaddr, 0, pg_cnt * PG_SIZE);

    lock_release(&user_pool.lock);

    return vaddr;
}

// 将地址vaddr与pf池中的物理地址关联,仅支持一页空间分配
void *get_a_page(enum pool_flags pf, uint32_t vaddr)
{
    struct pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);

    // 先将虚拟地址对应的位图置1
    struct task_struct *cur = running_thread();
    int32_t bit_idx         = -1;

    // 若当前是用户进程申请用户内存,就修改用户进程自己的虚拟地址位图
    if (cur->pgdir != NULL && pf == PF_USER)
    {
        bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);
    }
    else if (cur->pgdir == NULL && pf == PF_KERNEL)
    {
        // 如果是内核线程申请内核内存,就修改kernel_vaddr. 
        bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
        
    }
    else
    {
        PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
    }

    void *page_phyaddr = palloc(mem_pool);

    if (page_phyaddr == NULL)
    {
        return NULL;
    }

    page_table_add((void *)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);

    return (void *)vaddr;
}

// 安装1页大小的vaddr,专门针对fork时虚拟地址位图无须操作的情况
void *get_a_page_without_op_vaddrbitmap(enum pool_flags pf, uint32_t vaddr)
{
    struct pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

    lock_acquire(&mem_pool->lock);
    void *page_phyaddr = palloc(mem_pool);

    if (page_phyaddr == NULL)
    {
        lock_release(&mem_pool->lock);
        return NULL;
    }

    page_table_add((void *)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);

    return (void *)vaddr;
}

// 得到虚拟地址映射到的物理地址
uint32_t addr_v2p(uint32_t vaddr)
{
    uint32_t *pte = pte_ptr(vaddr);

    // (*pte)的值是页表所在的物理页框地址,去掉其低12位的页表项属性+虚拟地址vaddr的低12位
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

// 返回arena中第idx个内存块的地址
static struct mem_block *arena2block(struct arena *a, uint32_t idx)
{
    // arena是从堆中创建的，方法是使arena结构体指向从堆中返回的一个或多个页框的内存
    return (struct mem_block *)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

// 返回内存块b所在的arena地址
static struct arena *block2arena(struct mem_block *b)
{
    return (struct arena *)((uint32_t)b & 0xfffff000);
}

// 在堆中申请size字节内存
void *sys_malloc(uint32_t size)
{
    enum pool_flags PF;
    uint32_t pool_size;

    struct pool           *mem_pool;
    struct mem_block_desc *descs;
    struct task_struct    *cur_thread = running_thread();

    // 判断用哪个内存池
    if (cur_thread->pgdir == NULL) // 若为内核线程
    {
        PF = PF_KERNEL;
        pool_size = kernel_pool.pool_size;
        mem_pool = &kernel_pool;
        descs = k_block_descs;
    }
    else
    {   // 用户进程pcb中的pgdir会在为其分配页表时创建
        PF = PF_USER;
        pool_size = user_pool.pool_size;
        mem_pool = &user_pool;
        descs = cur_thread->u_block_desc;

    }

    // 若申请的内存不在内存池容量范围内则直接返回NULL
    if (!(size > 0 && size < pool_size))
    {
        return NULL;
    }

    struct arena     *a;
    struct mem_block *b;
    lock_acquire(&mem_pool->lock);

    // 超过最大内存块1024, 就分配页框
    if (size > 1024)
    {
        // 向上取整需要的页框数
        uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE); 

        a = malloc_page(PF, page_cnt); // 从堆中创建arena

        if (a != NULL)
        {
            memset(a, 0, page_cnt * PG_SIZE); // 将分配的内存清0

            // 对于分配的大块页框,将desc置为NULL, cnt置为页框数,large置为true
            a->desc  = NULL;
            a->cnt   = page_cnt;
            a->large = true;

            lock_release(&mem_pool->lock);

            // 此地址便是用户分配的内存地址
            // 跨过arena大小，把剩下的内存返回
            return (void *)(a + 1); 
        }
        else
        {
            lock_release(&mem_pool->lock);

            return NULL;
        }
    }
    else
    {   // 若申请的内存小于等于1024,可在各种规格的mem_block_desc中去适配

        uint8_t desc_idx; // 内存的索引

        // 从内存块描述符中匹配合适的内存块规格
        for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++)
        {
            if (size <= descs[desc_idx].block_size) // 从小往大后,找到后退出
            {
                break;
            }
        }

        // 若mem_block_desc的free_list中已经没有可用的mem_block,就创建新的arena提供mem_block
        if (list_empty(&descs[desc_idx].free_list))
        {
            // 分配1页框做为arena
            a = malloc_page(PF, 1); 

            if (a == NULL)
            {
                lock_release(&mem_pool->lock);
                return NULL;
            }

            // 为新的arena初始化信息，这次的arena用于小内存块分配
            memset(a, 0, PG_SIZE);

            // 对于分配的小块内存,将desc置为相应内存块描述符, cnt置为此arena可用的内存块数,large置为false
            a->desc  = &descs[desc_idx];             // 使desc指向上面找到的内存块描述符
            a->large = false;
            a->cnt   = descs[desc_idx].blocks_per_arena;

            uint32_t block_idx;

            enum intr_status old_status = intr_disable();

            //  开始将arena拆分成内存块,并添加到内存块描述符的free_list中
            for (block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena; block_idx++)
            {
                b = arena2block(a, block_idx);
                ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
                list_append(&a->desc->free_list, &b->free_elem);
            }

            intr_set_status(old_status);

        } // end if

        // 开始分配内存块
        b = elem2entry(struct mem_block, free_elem, list_pop(&(descs[desc_idx].free_list)));
        memset(b, 0, descs[desc_idx].block_size);

        a = block2arena(b);             // 获取内存块b所在的arena
        a->cnt--;                       // 将此arena中的空闲内存块数减1

        lock_release(&mem_pool->lock);

        return (void *)b;

    } // end if

}

// 将物理地址pg_phy_addr回收到物理内存池
void pfree(uint32_t pg_phy_addr)
{
    struct pool *mem_pool;
    uint32_t bit_idx = 0;

    if (pg_phy_addr >= user_pool.phy_addr_start)             // 用户物理内存池
    {
        mem_pool = &user_pool;
        bit_idx  = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
    }
    else
    {   // 内核物理内存池
        mem_pool = &kernel_pool;
        bit_idx  = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
    }

    // 将位图中该位清0
    bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0); 

    return ;
}

// 去掉页表中虚拟地址vaddr的映射,只去掉vaddr对应的pte
static void page_table_pte_remove(uint32_t vaddr)
{
    uint32_t *pte = pte_ptr(vaddr);
    *pte &= ~PG_P_1;     // 将页表项pte的P位置0

    // 更新tlb
    asm volatile("invlpg %0" ::"m"(vaddr)
                 : "memory"); 
                
    return ;
}

// 在虚拟地址池中释放以_vaddr起始的连续pg_cnt个虚拟页地址
static void vaddr_remove(enum pool_flags pf, void *_vaddr, uint32_t pg_cnt)
{
    uint32_t bit_idx_start = 0, vaddr = (uint32_t)_vaddr, cnt = 0;

    if (pf == PF_KERNEL)         // 内核虚拟内存池
    {
        bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;

        while (cnt < pg_cnt)
        {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }

    }
    else
    {   // 用户虚拟内存池

        struct task_struct *cur_thread = running_thread();
        bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;

        while (cnt < pg_cnt)
        {
            bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }

    }

    return ;
}

// 释放以虚拟地址vaddr为起始的cnt个物理页框
void mfree_page(enum pool_flags pf, void *_vaddr, uint32_t pg_cnt)
{
    uint32_t pg_phy_addr;
    uint32_t vaddr = (int32_t)_vaddr, page_cnt = 0;

    ASSERT(pg_cnt >= 1 && vaddr % PG_SIZE == 0);

    // 获取虚拟地址vaddr对应的物理地址
    pg_phy_addr = addr_v2p(vaddr); 

    // 确保待释放的物理内存在低端1M+1k大小的页目录+1k大小的页表地址范围外
    ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= 0x102000);

    // 判断pg_phy_addr属于用户物理内存池还是内核物理内存池
    if (pg_phy_addr >= user_pool.phy_addr_start)      // 位于user_pool内存池
    {
        vaddr -= PG_SIZE;

        while (page_cnt < pg_cnt)
        {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);

            // 确保物理地址属于用户物理内存池
            ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= user_pool.phy_addr_start);

            // 先将对应的物理页框归还到内存池
            pfree(pg_phy_addr);

            // 再从页表中清除此虚拟地址所在的页表项pte
            page_table_pte_remove(vaddr);

            page_cnt++;
        }

        // 清空虚拟地址的位图中的相应位
        vaddr_remove(pf, _vaddr, pg_cnt);
    }
    else
    {   // 位于kernel_pool内存池

        vaddr -= PG_SIZE;

        while (page_cnt < pg_cnt)
        {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);

            // 确保待释放的物理内存只属于内核物理内存池
            ASSERT((pg_phy_addr % PG_SIZE) == 0              &&
                   pg_phy_addr >= kernel_pool.phy_addr_start &&
                   pg_phy_addr < user_pool.phy_addr_start);

            // 先将对应的物理页框归还到内存池
            pfree(pg_phy_addr);

            // 再从页表中清除此虚拟地址所在的页表项pte
            page_table_pte_remove(vaddr);

            page_cnt++;
        }

        // 清空虚拟地址的位图中的相应位
        vaddr_remove(pf, _vaddr, pg_cnt);

    } // end if

    return ;
}

// 回收内存ptr
void sys_free(void *ptr)
{
    ASSERT(ptr != NULL);

    if (ptr != NULL)
    {

        enum pool_flags PF;
        struct pool *mem_pool;

        // 判断是线程还是进程
        if (running_thread()->pgdir == NULL)
        {
            ASSERT((uint32_t)ptr >= K_HEAP_START);
            PF = PF_KERNEL;
            mem_pool = &kernel_pool;
        }
        else
        {
            PF = PF_USER;
            mem_pool = &user_pool;
        }

        lock_acquire(&mem_pool->lock);

        struct mem_block *b = ptr;
        struct arena *a     = block2arena(b);     // 把mem_block转换成arena,获取元信息

        ASSERT(a->large == 0 || a->large == 1);

        if (a->desc == NULL && a->large == true)
        {   // 大于1024的内存
            mfree_page(PF, a, a->cnt);
        }
        else
        {   // 小于等于1024的内存块
            // 先将内存块回收到free_list
            list_append(&a->desc->free_list, &b->free_elem);

            // 再判断此arena中的内存块是否都是空闲,如果是就释放arena
            if (++a->cnt == a->desc->blocks_per_arena)
            {
                uint32_t block_idx;
                for (block_idx = 0; block_idx < a->desc->blocks_per_arena; block_idx++)
                {
                    struct mem_block *b = arena2block(a, block_idx);
                    ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
                    list_remove(&b->free_elem);
                }

                mfree_page(PF, a, 1);
            }

        }

        // end if
        lock_release(&mem_pool->lock);

    } //end if

    return ;
}

// 为malloc做准备
void block_desc_init(struct mem_block_desc *desc_array)
{
    uint16_t desc_idx, block_size = 16;

    // 初始化每个mem_block_desc描述符
    for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++)
    {
        desc_array[desc_idx].block_size = block_size;

        // 初始化arena中的内存块数量
        desc_array[desc_idx].blocks_per_arena = (PG_SIZE - sizeof(struct arena)) / block_size;

        list_init(&desc_array[desc_idx].free_list);

        block_size *= 2; // 更新为下一个规格内存块
    }

    return ;
}

// 初始化内存池
static void mem_pool_init(uint32_t all_mem)
{
    put_str("  mem_pool_init start\n");

    // 页表大小= 1页的页目录表+第0和第768个页目录项指向同一个页表+第769~1022个页目录项共指向254个页表,共256个页框
    uint32_t page_table_size   = PG_SIZE * 256;

    // 0x100000为低端1M内存,用来记录当前已使用的内存字节数
    uint32_t used_mem          = page_table_size + 0x100000; 

    // 用来存储目前可以用的内存字节数
    uint32_t free_mem          = all_mem - used_mem;         

    // 1页为4k,不管总内存是不是4k的倍数,对于以页为单位的内存分配策略，不足1页的内存不用考虑了。
    uint16_t all_free_pages    = free_mem / PG_SIZE;    // 用来保存可用内存字节数free_mem转换成的物理页数
    uint16_t kernel_free_pages = all_free_pages / 2;    // kernel
    uint16_t user_free_pages   = all_free_pages - kernel_free_pages;

    /**
     * @brief 
     * 为简化位图操作，余数不处理，坏处是这样做会丢内存。
     * 好处是不用做内存的越界检查,因为位图表示的内存少于实际物理内存
     * 
     */

    uint32_t kbm_length = kernel_free_pages / 8;  // Kernel BitMap的长度,位图中的一位表示一页,以字节为单位
    uint32_t ubm_length = user_free_pages / 8;    // User BitMap的长度

    uint32_t kp_start   = used_mem;               // Kernel Pool start,内核内存池的起始地址

    // User Pool start,用户内存池的起始地址
    uint32_t up_start   = kp_start + kernel_free_pages * PG_SIZE;

    kernel_pool.phy_addr_start = kp_start;         // 用于记录内核物理内存池的起始地址
    user_pool.phy_addr_start = up_start;           // 用户物理内存池的起始地址

    // 两个起始物理地址初始化各自内存池的起始地址
    kernel_pool.pool_size = kernel_free_pages * PG_SIZE; // kernel
    user_pool.pool_size = user_free_pages * PG_SIZE;     // User

    // 初始化各自内存池中的容量宇节数
    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length; // kernel
    user_pool.pool_bitmap.btmp_bytes_len = ubm_length;   // User

    /**
     * @brief Create 是初始化各自内存池所使用的位图。
     * 位图是全局的数据，长度不固定。
     * 全局或静态的数组需要在编译时知道其长度，
     * 而我们需要根据总内存大小算出需要多少字节。
     * 所以改为指定一块内存来生成位图.
     */

    // 内核使用的最高地址是0xc009f000,这是主线程的栈地址.(内核的大小预计为70K左右
    // 32M内存占用的位图是2k.内核内存池的位图先定在MEM_BITMAP_BASE(0xc009a000)处
    kernel_pool.pool_bitmap.bits = (void *)MEM_BITMAP_BASE;              

    // 用户内存池的位图紧跟在内核内存池位图之后
    user_pool.pool_bitmap.bits   = (void *)(MEM_BITMAP_BASE + kbm_length); 

    // 输出内存池信息
    put_str("  kernel_pool_bitmap_start: ");
    put_int((int)kernel_pool.pool_bitmap.bits);

    put_str("  kernel_pool_phy_addr_start: ");
    put_int(kernel_pool.phy_addr_start);

    put_str("\n");

    put_str("  user_pool_bitmap_start:   ");
    put_int((int)user_pool.pool_bitmap.bits);

    put_str("  user_pool_phy_addr_start:   ");
    put_int(user_pool.phy_addr_start);
    
    put_str("\n");

    // 将位图置0
    bitmap_init(&kernel_pool.pool_bitmap);     // kernel
    bitmap_init(&user_pool.pool_bitmap);       // user

    lock_init(&kernel_pool.lock);              // kernel，添加内核锁
    lock_init(&user_pool.lock);                // user，添加用户锁

    // 下面初始化内核虚拟地址的位图,按实际物理内存大小生成数组。
    // 用于维护内核堆的虚拟地址,所以要和内核内存池大小一致
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length; 

    // 位图的数组指向一块未使用的内存,目前定位在内核内存池和用户内存池之外
    kernel_vaddr.vaddr_bitmap.bits = (void *)(MEM_BITMAP_BASE + kbm_length + ubm_length);

    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);

    put_str("  mem_pool_init done\n");

    return ;
}

// 根据物理页框地址pg_phy_addr在相应的内存池的位图清0,不改动页表
void free_a_phy_page(uint32_t pg_phy_addr)
{
    struct pool *mem_pool;
    uint32_t bit_idx = 0;

    if (pg_phy_addr >= user_pool.phy_addr_start)
    {
        mem_pool = &user_pool;
        bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
    }
    else
    {
        mem_pool = &kernel_pool;
        bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
    }

    bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);

    return ;
}

// 内存管理部分初始化入口
void mem_init(void)
{
    put_str("\nmem_init start\n");
    uint32_t mem_bytes_total = (*(uint32_t *)(0xb00));
    mem_pool_init(mem_bytes_total); // 初始化内存池

    // 初始化mem_block_desc数组descs,为malloc做准备
    block_desc_init(k_block_descs);
    put_str("mem_init done\n\n");

    return;
}

/**
     * @brief
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
