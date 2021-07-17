#include "dir.h"
#include "inode.h"
#include "file.h"
#include "fs.h"
#include "stdint.h"
#include "super-block.h"
#include "../lib/string.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../kernel/global.h"
#include "../kernel/debug.h"
#include "../kernel/memory.h"
#include "../kernel/interrupt.h"


struct dir root_dir;        // 根目录

// 打开分区part根目录
void open_root_dir(struct partition *part)
{
    root_dir.inode   = inode_open(part, part->sb->root_inode_no);
    root_dir.dir_pos = 0;

    return;
}

// 在分区part上打开i结点为inode_no的目录并返回目录指针
struct dir *dir_open(struct partition *part, uint32_t inode_no)
{
    struct dir *pdir = (struct dir *)sys_malloc(sizeof(struct dir));

    pdir->inode      = inode_open(part, inode_no);
    pdir->dir_pos    = 0;

    return pdir;
}

// 在part分区内的pdir目录内寻找名为name的文件或目录,
// 找到后返回true并将其目录项存入dir_e,否则返回false
bool search_dir_entry(struct partition *part, struct dir *pdir, const char *name, struct dir_entry *dir_e)
{
    uint32_t block_cnt = 140;                                // 12个直接块+128个一级间接块=140块

    // 12个直接块大小+128个间接块,共560字节
    uint32_t *all_blocks = (uint32_t *)sys_malloc(48 + 512); // 分配140个扇区内存

    if (all_blocks == NULL)
    {
        printk("search_dir_entry: sys_malloc for all_blocks failed");
        return false;
    }

    // 先将目录inode的前i_sectors中的前12个扇区地址都统一从all_blocks中录取
    uint32_t block_idx = 0;
    while (block_idx < 12)
    {
        all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
        block_idx++;
    }
    block_idx = 0;

    // 判断时候有一级间接块索引表
    // 至此,all_blocks存储的是该文件或目录的所有扇区地址
    if (pdir->inode->i_sectors[12] != 0) // 若含有一级间接块表
    {
        ide_read(part->my_disk, pdir->inode->i_sectors[12], all_blocks + 12, 1);
    }

    // 写目录项的时候已保证目录项不跨扇区，这样读目录项时容易处理, 只申请容纳1个扇区的内存
    uint8_t *buf = (uint8_t *)sys_malloc(SECTOR_SIZE);      // 为缓冲区申请一个扇区的内存
    struct dir_entry *p_de  = (struct dir_entry *)buf;      // p_de为指向目录项的指针,值为buf起始地址
    uint32_t dir_entry_size = part->sb->dir_entry_size;
    uint32_t dir_entry_cnt  = SECTOR_SIZE / dir_entry_size; // 1扇区内可容纳的目录项个数

    // 开始在所有块中查找目录项
    while (block_idx < block_cnt)
    {
        // 块地址为0时表示该块中无数据,继续在其它块中找
        if (all_blocks[block_idx] == 0)
        {
            block_idx++;
            continue;
        }

        ide_read(part->my_disk, all_blocks[block_idx], buf, 1);

        uint32_t dir_entry_idx = 0;

        // 遍历扇区中所有目录项
        while (dir_entry_idx < dir_entry_cnt)
        {
            // 若找到了,就直接复制整个目录项
            if (!strcmp(p_de->filename, name))
            {
                memcpy(dir_e, p_de, dir_entry_size);

                sys_free(buf);
                sys_free(all_blocks);
                return true;
            }

            dir_entry_idx++;
            p_de++;
        }

        block_idx++;
        p_de = (struct dir_entry *)buf;         // 此时p_de已经指向扇区内最后一个完整目录项了,需要恢复p_de指向为buf

        memset(buf, 0, SECTOR_SIZE);            // 将buf清0,下次再用

    } // end while

    sys_free(buf);
    sys_free(all_blocks);

    return false;
}

// 关闭目录
void dir_close(struct dir *dir)
{
    /**
     * @brief 根目录不能关闭
     * 1 根目录自打开后就不应该关闭,否则还需要再次open_root_dir();
     * 2 root_dir所在的内存是低端1M之内,并非在堆中,free会出问题
     * 
     */

    if (dir == &root_dir)
    {
        return;                        // 不做任何处理直接返回
    }

    inode_close(dir->inode);
    sys_free(dir);

    return;
}

// 在内存中初始化目录项p_de
void create_dir_entry(char *filename, uint32_t inode_no, uint8_t file_type, struct dir_entry *p_de)
{
    ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);

    // 初始化目录项
    memcpy(p_de->filename, filename, strlen(filename));
    p_de->i_no   = inode_no;
    p_de->f_type = file_type;

    return;
}

// 将目录项p_de写入父目录parent_dir中,io_buf由主调函数提
bool sync_dir_entry(struct dir *parent_dir, struct dir_entry *p_de, void *io_buf)
{
    struct inode *dir_inode = parent_dir->inode;
    uint32_t dir_size       = dir_inode->i_size;                  // 目录中目录项大小之和
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;       // 获取超级块的大小

    ASSERT(dir_size % dir_entry_size == 0); // dir_size应该是dir_entry_size的整数倍

    uint32_t dir_entry_per_sec = (512 / dir_entry_size);         // 每扇区最大的目录项数目
    int32_t block_lba          = -1;

    // 将该目录的所有扇区地址(12个直接块+ 128个间接块)存入all_blocks
    uint8_t block_idx        = 0;
    uint32_t all_blocks[140] = {0}; // all_blocks保存目录所有的块

    // 将12个直接块存入all_blocks
    while (block_idx < 12)
    {
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }

    struct dir_entry *dir_e  = (struct dir_entry *)io_buf;      // dir_e用来在io_buf中遍历目录项
    int32_t block_bitmap_idx = -1;

    // 开始遍历所有块以寻找目录项空位,若已有扇区中没有空闲位,在不超过文件大小的情况下申请新扇区来存储新目录项
    block_idx = 0;

    while (block_idx < 140)             // 文件(包括目录)最大支持12个直接块+128个间接块＝140个块
    {
        block_bitmap_idx = -1;

        if (all_blocks[block_idx] == 0) // 在三种情况下分配块
        {
            // 分配一个扇区
            block_lba = block_bitmap_alloc(cur_part);

            if (block_lba == -1)
            {
                printk("alloc block bitmap for sync_dir_entry failed\n");
                return false;
            }

            // 每分配一个块就同步一次block_bitmap
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
            ASSERT(block_bitmap_idx != -1);

            // 将位图同步到硬盘
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

            block_bitmap_idx = -1;

            if (block_idx < 12) // 若是直接块
            {
                dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
            }
            else if (block_idx == 12)
            { // 若是尚未分配一级间接块表(block_idx等于12表示第0个间接块地址为0)

                dir_inode->i_sectors[12] = block_lba; // 将上面分配的块做为一级间接块表地址
                block_lba = -1;
                block_lba = block_bitmap_alloc(cur_part); // 再分配一个块做为第0个间接块

                if (block_lba == -1)
                {
                    block_bitmap_idx = dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
                    bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
                    dir_inode->i_sectors[12] = 0;
                    printk("alloc block bitmap for sync_dir_entry failed\n");

                    return false;
                }

                // 每分配一个块就同步一次block_bitmap
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                ASSERT(block_bitmap_idx != -1);
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

                all_blocks[12] = block_lba;

                // 把新分配的第0个间接块地址写入一级间接块表
                ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
            }
            else
            {         // 若是间接块未分配

                all_blocks[block_idx] = block_lba;

                // 把新分配的第(block_idx-12)个间接块地址写入一级间接块表
                ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);

            } // end if

            // 再将新目录项p_de写入新分配的间接块
            memset(io_buf, 0, 512);
            memcpy(io_buf, p_de, dir_entry_size);

            ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
            dir_inode->i_size += dir_entry_size;

            return true;

        } //end if

        // 若第block_idx块已存在,将其读进内存,然后在该块中查找空目录项
        ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);

        // 在扇区内查找空目录项
        uint8_t dir_entry_idx = 0;

        while (dir_entry_idx < dir_entry_per_sec)
        {
            // FT_UNKNOWN为0,无论是初始化或是删除文件后,都会将f_type置为FT_UNKNOWN.
            if ((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN)
            {
                memcpy(dir_e + dir_entry_idx, p_de, dir_entry_size);
                ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);

                dir_inode->i_size += dir_entry_size;
                return true;
            }

            dir_entry_idx++;
        }
        block_idx++;

    } // end while

    printk("directory is full!\n");

    return false;
}

// 把分区part目录pdir中编号为inode_no的目录项删除
bool delete_dir_entry(struct partition *part, struct dir *pdir, uint32_t inode_no, void *io_buf)
{
    /**
     * @brief 删除目录的工作
     * 
     * 1. 在文件所在的目录在中擦除该文件的目录项，使其为0
     * 2. 根目录是必须存在的，它是文件读写的根基，不应该被清空，它至少要保留1个块。
     *    如果目录项独占1块，并且该块不是根目录最后一个块的话，将其回收。
     * 3. 目录inode的i_size是目录项大小的总和，因此还要将i_size减去一个目录项的单位大小。
     * 4. 目录 inode 改变后，要同步到硬盘。
     * 
     */

    struct inode *dir_inode = pdir->inode;
    uint32_t block_idx = 0, all_blocks[140] = {0};

    // 收集目录全部块地址
    while (block_idx < 12)
    {
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }

    if (dir_inode->i_sectors[12])
    {
        ide_read(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
    }

    // 目录项在存储时保证不会跨扇区
    uint32_t dir_entry_size    = part->sb->dir_entry_size;
    uint32_t dir_entry_per_sec = (SECTOR_SIZE / dir_entry_size);   // 每扇区最大的目录项数目

    struct dir_entry *dir_e    = (struct dir_entry *)io_buf;
    struct dir_entry *dir_entry_found = NULL;

    uint8_t dir_entry_idx, dir_entry_cnt;
    bool is_dir_first_block = false; // 目录的第1个块

    // 遍历所有块,寻找目录项
    block_idx = 0;
    while (block_idx < 140)
    {
        is_dir_first_block = false;

        if (all_blocks[block_idx] == 0)
        {
            block_idx++;
            continue;
        }

        dir_entry_idx = dir_entry_cnt = 0;

        memset(io_buf, 0, SECTOR_SIZE);

        // 读取扇区,获得目录项
        ide_read(part->my_disk, all_blocks[block_idx], io_buf, 1);

        // 遍历所有的目录项,统计该扇区的目录项数量及是否有待删除的目录项
        while (dir_entry_idx < dir_entry_per_sec)
        {

            if ((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN)
            {
                if (!strcmp((dir_e + dir_entry_idx)->filename, "."))
                {
                    is_dir_first_block = true;
                }
                else if (strcmp((dir_e + dir_entry_idx)->filename, ".") &&
                         strcmp((dir_e + dir_entry_idx)->filename, ".."))
                {
                    // 统计此扇区内的目录项个数,用来判断删除目录项后是否回收该扇区
                    dir_entry_cnt++; 

                    // 如果找到此i结点,就将其记录在dir_entry_found
                    if ((dir_e + dir_entry_idx)->i_no == inode_no)
                    {
                        // 确保目录中只有一个编号为inode_no的inode,找到一次后dir_entry_found就不再是NULL
                        ASSERT(dir_entry_found == NULL);

                        dir_entry_found = dir_e + dir_entry_idx;

                        // 找到后也继续遍历,统计总共的目录项数
                    }
                }

            } // end if

            dir_entry_idx++;
        }

        // 若此扇区未找到该目录项,继续在下个扇区中找
        if (dir_entry_found == NULL)
        {
            block_idx++;
            continue;
        }

        // 在此扇区中找到目录项后,清除该目录项并判断是否回收扇区,随后退出循环直接返回
        ASSERT(dir_entry_cnt >= 1);

        // 除目录第1个扇区外,若该扇区上只有该目录项自己,则将整个扇区回收
        if (dir_entry_cnt == 1 && !is_dir_first_block)
        {
            // A 在块位图中回收该块
            uint32_t block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
            bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

            // B 将块地址从数组i_sectors或索引表中去掉
            if (block_idx < 12)
            {
                dir_inode->i_sectors[block_idx] = 0;
            }
            else
            { // 在一级间接索引表中擦除该间接块地址

                // 先判断一级间接索引表中间接块的数量,如果仅有这1个间接块,连同间接索引表所在的块一同回收
                uint32_t indirect_blocks = 0;
                uint32_t indirect_block_idx = 12;

                while (indirect_block_idx < 140)
                {
                    if (all_blocks[indirect_block_idx] != 0)
                    {
                        indirect_blocks++;
                    }
                }

                // 包括当前间接块
                ASSERT(indirect_blocks >= 1);

                // 间接索引表中还包括其它间接块,仅在索引表中擦除当前这个间接块地址
                if (indirect_blocks > 1)
                {
                    all_blocks[block_idx] = 0;
                    ide_write(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
                }
                else
                {          // 间接索引表中就当前这1个间接块,直接把间接索引表所在的块回收,然后擦除间接索引表块地址

                    // 回收间接索引表所在的块
                    block_bitmap_idx = dir_inode->i_sectors[12] - part->sb->data_start_lba;

                    bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
                    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

                    // 将间接索引表地址清0
                    dir_inode->i_sectors[12] = 0;
                }

            } // end if
        }
        else
        { // 仅将该目录项清空
            memset(dir_entry_found, 0, dir_entry_size);
            ide_write(part->my_disk, all_blocks[block_idx], io_buf, 1);
        }

        // 更新i结点信息并同步到硬盘
        ASSERT(dir_inode->i_size >= dir_entry_size);

        dir_inode->i_size -= dir_entry_size;
        memset(io_buf, 0, SECTOR_SIZE * 2);
        inode_sync(part, dir_inode, io_buf);

        return true;
    }

    // 所有块中未找到则返回false,若出现这种情况应该是serarch_file出错了
    return false;
}

// 读取目录,成功返回1个目录项,失败返回NULL
struct dir_entry *dir_read(struct dir *dir)
{
    struct dir_entry *dir_e = (struct dir_entry *)dir->dir_buf;
    struct inode *dir_inode = dir->inode;

    uint32_t all_blocks[140] = {0}, block_cnt = 12;
    uint32_t block_idx = 0, dir_entry_idx = 0;

    while (block_idx < 12)
    {
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }

    if (dir_inode->i_sectors[12] != 0) // 若含有一级间接块表
    {
        ide_read(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
        block_cnt = 140;
    }
    block_idx = 0;

    // 当前目录项的偏移,此项用来判断是否是之前已经返回过的目录项
    uint32_t cur_dir_entry_pos = 0;
    uint32_t dir_entry_size    = cur_part->sb->dir_entry_size;
    uint32_t dir_entry_per_sec = SECTOR_SIZE / dir_entry_size; // 1扇区内可容纳的目录项个数

    // 因为此目录内可能删除了某些文件或子目录,所以要遍历所有块
    while (block_idx < block_cnt)
    {

        if (dir->dir_pos >= dir_inode->i_size)
        {
            return NULL;
        }

        // 如果此块地址为0,即空块,继续读出下一块
        if (all_blocks[block_idx] == 0)
        {
            block_idx++;
            continue;
        }

        memset(dir_e, 0, SECTOR_SIZE);
        ide_read(cur_part->my_disk, all_blocks[block_idx], dir_e, 1);
        dir_entry_idx = 0;

        // 遍历扇区内所有目录项
        while (dir_entry_idx < dir_entry_per_sec)
        {
            // 如果f_type不等于0,即不等于FT_UNKNOWN
            if ((dir_e + dir_entry_idx)->f_type)
            {
                // 判断是不是最新的目录项,避免返回曾经已经返回过的目录项
                if (cur_dir_entry_pos < dir->dir_pos)
                {
                    cur_dir_entry_pos += dir_entry_size;
                    dir_entry_idx++;
                    continue;
                }

                ASSERT(cur_dir_entry_pos == dir->dir_pos);

                // 更新为新位置,即下一个返回的目录项地址
                dir->dir_pos += dir_entry_size;

                return dir_e + dir_entry_idx;
            }

            dir_entry_idx++;
        }

        block_idx++;
        
    }

    return NULL;
}

// 判断目录是否为空
bool dir_is_empty(struct dir *dir)
{
    // 若目录下只有.和..这两个目录项则目录为空
    struct inode *dir_inode = dir->inode;

    return (dir_inode->i_size == cur_part->sb->dir_entry_size * 2);
}

// 在父目录parent_dir中删除child_dir
int32_t dir_remove(struct dir *parent_dir, struct dir *child_dir)
{
    struct inode *child_dir_inode = child_dir->inode;

    // 空目录只在inode->i_sectors[0]中有扇区,其它扇区都应该为空
    int32_t block_idx = 1;

    while (block_idx < 13)
    {
        ASSERT(child_dir_inode->i_sectors[block_idx] == 0);
        block_idx++;
    }

    void *io_buf = sys_malloc(SECTOR_SIZE * 2);
    if (io_buf == NULL)
    {
        printk("dir_remove: malloc for io_buf failed\n");
        return -1;
    }

    // 在父目录parent_dir中删除子目录child_dir对应的目录项
    delete_dir_entry(cur_part, parent_dir, child_dir_inode->i_no, io_buf);

    // 回收inode中i_sector中所占用的扇区,并同步inode_bitmap和block_bitmap
    inode_release(cur_part, child_dir_inode->i_no);
    sys_free(io_buf);

    return 0;
}
