/**
 * seqlist.c —— 存储层实现
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "seqlist.h"

#include <stdlib.h>
#include <string.h>

SeqList* List_Create(int capacity)
{
    SeqList* L;

    if (capacity <= 0) capacity = LIST_INIT_CAPACITY;

    L = (SeqList*)malloc(sizeof(SeqList));
    if (!L) return NULL;

    L->data = (Doctor*)malloc(sizeof(Doctor) * (size_t)capacity);
    if (!L->data) {
        free(L);
        return NULL;
    }

    L->length   = 0;
    L->capacity = capacity;
    return L;
}

void List_Destroy(SeqList* L)
{
    if (!L) return;
    free(L->data);
    free(L);
}

int List_Reserve(SeqList* L, int needCapacity)
{
    int     cap;
    Doctor* p;

    if (!L) return 0;
    if (needCapacity <= L->capacity) return 1;

    cap = (L->capacity > 0) ? L->capacity : LIST_INIT_CAPACITY;
    while (cap < needCapacity) {
        if (cap > (int)(0x7FFFFFFF / 2)) return 0;   /* 防溢出 */
        cap *= 2;
    }

    p = (Doctor*)realloc(L->data, sizeof(Doctor) * (size_t)cap);
    if (!p) return 0;

    L->data     = p;
    L->capacity = cap;
    return 1;
}

int List_Insert(SeqList* L, int pos, const Doctor* e)
{
    if (!L || !e)                 return 0;
    if (pos < 0 || pos > L->length) return 0;
    if (!List_Reserve(L, L->length + 1)) return 0;

    /* 将 [pos, length) 整体后移一格 */
    memmove(&L->data[pos + 1], &L->data[pos],
            (size_t)(L->length - pos) * sizeof(Doctor));

    L->data[pos] = *e;
    L->length++;
    return 1;
}

int List_Append(SeqList* L, const Doctor* e)
{
    if (!L) return 0;
    return List_Insert(L, L->length, e);
}

int List_Delete(SeqList* L, int pos)
{
    if (!L || pos < 0 || pos >= L->length) return 0;

    /* 将 (pos, length) 整体前移一格 */
    memmove(&L->data[pos], &L->data[pos + 1],
            (size_t)(L->length - pos - 1) * sizeof(Doctor));

    L->length--;
    return 1;
}

Doctor* List_At(SeqList* L, int pos)
{
    if (!L || pos < 0 || pos >= L->length) return NULL;
    return &L->data[pos];
}

int List_FindById(const SeqList* L, const char* id)
{
    int i;

    if (!L || !id) return -1;
    /* 排序操作会改变物理顺序，故统一使用顺序查找 */
    for (i = 0; i < L->length; i++) {
        if (strcmp(L->data[i].id, id) == 0) return i;
    }
    return -1;
}

void List_Clear(SeqList* L)
{
    if (L) L->length = 0;
}

int List_IsEmpty(const SeqList* L)
{
    return (!L || L->length == 0);
}
