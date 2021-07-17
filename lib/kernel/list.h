#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H
#include "../kernel/global.h"

// 将结点元素转为实际元素项
#define offset(struct_type, member) (int)(&((struct_type *)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr) \
    (struct_type *)((int)elem_ptr - offset(struct_type, struct_member_name))

// 定义链表结点成员结构，结点中不需要数据成元,只要求前驱和后继结点指针
struct list_elem
{
    struct list_elem *prev;         // 前驱结点
    struct list_elem *next;         // 后继结点
};

// 链表结构,用来实现队列
struct list
{
    struct list_elem head;          // head是队首,是固定不变的，不是第1个元素,第1个元素为head.next
    struct list_elem tail;          // tail是队尾,同样是固定不变的
};


// 自定义函数类型function,用于在list_traversal中做回调函数
typedef bool(function)(struct list_elem *, int arg); 

// 初始化双向链表list
void list_init(struct list *);

// 把链表元素elem插入在元素before之前
void list_insert_before(struct list_elem *before, struct list_elem *elem);

// 添加元素到列表队首,类似栈push操作
void list_push(struct list *plist, struct list_elem *elem);

void list_iterate(struct list *plist);

// 追加元素到链表队尾,类似队列的先进先出操作
void list_append(struct list *plist, struct list_elem *elem);

// 使元素pelem脱离链表
void list_remove(struct list_elem *pelem);

// 将链表第一个元素弹出并返回,类似栈的pop操作
struct list_elem *list_pop(struct list *plist);

// 判断链表是否为空,空时返回true,否则返回false
bool list_empty(struct list *plist);

// 返回链表长度
uint32_t list_len(struct list *plist);

// 本函数的功能是遍历列表内所有元素,逐个判断是否有符合条件的元素
struct list_elem *list_traversal(struct list *plist, function func, int arg);

// 从链表中查找元素obj_elem,成功时返回true,失败时返回false
bool elem_find(struct list *plist, struct list_elem *obj_elem);

#endif // __LIB_KERNEL_LIST_H