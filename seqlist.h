/**
 * seqlist.h —— 存储层
 * 顺序表(动态数组)：底层连续存储 + 容量倍增扩容。
 */
#ifndef SEQLIST_H
#define SEQLIST_H

#include "doctor.h"

#define LIST_INIT_CAPACITY 16

typedef struct {
    Doctor* data;       /* 连续存储区 */
    int     length;     /* 当前元素个数 */
    int     capacity;   /* 当前容量 */
} SeqList;

/* 创建/销毁 */
SeqList* List_Create(int capacity);
void     List_Destroy(SeqList* L);

/* 容量管理：保证至少能容纳 needCapacity 个元素，内部按 2 倍增长 */
int      List_Reserve(SeqList* L, int needCapacity);

/* 增删 */
int      List_Insert(SeqList* L, int pos, const Doctor* e);   /* 在 pos 处插入，pos 合法范围 [0, length] */
int      List_Append(SeqList* L, const Doctor* e);            /* 尾部追加 */
int      List_Delete(SeqList* L, int pos);                    /* 删除 pos 处元素 */

/* 访问与查找 */
Doctor*  List_At(SeqList* L, int pos);                        /* 越界返回 NULL */
int      List_FindById(const SeqList* L, const char* id);     /* 返回下标，未找到返回 -1 */

/* 状态 */
void     List_Clear(SeqList* L);
int      List_IsEmpty(const SeqList* L);

#endif /* SEQLIST_H */
