/**
 * seqlist.h —— 存储层
 *
 * 顺序表(动态数组)：底层连续存储 + 容量倍增扩容。
 *
 * v2.1 说明：业务层采用「逻辑删除 + 空位复用」，因此常规删除不走 List_Delete，
 * 而是把记录的不外显标识符置为 SLOT_VACANT。List_Delete 仅用于彻底清理空位。
 */
#ifndef SEQLIST_H
#define SEQLIST_H

#include "doctor.h"

#define LIST_INIT_CAPACITY 16

typedef struct {
    Doctor* data;       /* 连续存储区，每个元素是一个“栏位” */
    int     length;     /* 已开辟的栏位总数（含空位） */
    int     capacity;   /* 当前容量 */
} SeqList;

/* 创建/销毁 */
SeqList* List_Create(int capacity);
void     List_Destroy(SeqList* L);

/* 容量管理：保证至少能容纳 needCapacity 个栏位，内部按 2 倍增长 */
int      List_Reserve(SeqList* L, int needCapacity);

/* 栏位操作 */
int      List_Insert(SeqList* L, int pos, const Doctor* e);   /* 在 pos 处开辟栏位并写入 */
int      List_Append(SeqList* L, const Doctor* e);            /* 在末尾真实开辟新栏位 */
int      List_Delete(SeqList* L, int pos);                    /* 物理移除栏位，后续栏位前移 */

/* 访问与查找 */
Doctor*  List_At(SeqList* L, int pos);                        /* 越界返回 NULL */
int      List_FindByWorkId(const SeqList* L, const char* workId);   /* 返回下标，未找到 -1 */

/* 状态 */
void     List_Clear(SeqList* L);
int      List_IsEmpty(const SeqList* L);

#endif /* SEQLIST_H */
