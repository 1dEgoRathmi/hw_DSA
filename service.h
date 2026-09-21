/**
 * service.h —— 业务层
 * 在顺序表之上封装：增删查改、区间查找、区间修改、区间统计、排序、文件读写。
 */
#ifndef SERVICE_H
#define SERVICE_H

#include "doctor.h"
#include "seqlist.h"

/* 区间查找/筛选的最大结果条数 */
#define MAX_RESULT 1024

/* 筛选结果集合：保存命中记录在顺序表中的下标 */
typedef struct {
    int idx[MAX_RESULT];
    int count;
} IndexSet;

/* 统计结果 */
typedef struct {
    int count;                              /* 人数 */
    int ageSum;                             /* 年龄总和，用于算平均值 */
    int minAge;
    int maxAge;
    int genderCount[GENDER_COUNT];
    int eduCount[EDU_COUNT];
    int deptCount[DEPT_COUNT];
    int titleCount[TITLE_COUNT];
    int fieldCount[FIELD_COUNT];
} Stats;

/* ===================== 基本增删查改 ===================== */
int     Service_Add(SeqList* L, const Doctor* d);        /* 1 成功；-1 工号重复；0 内存不足 */
int     Service_RemoveById(SeqList* L, const char* id);  /* 1 成功；0 未找到 */
Doctor* Service_FindById(SeqList* L, const char* id);
void    Service_FindByName(const SeqList* L, const char* name, IndexSet* out);  /* 姓名模糊匹配 */

/* ===================== 区间查找（筛选） ===================== */
void Service_SelectAll(const SeqList* L, IndexSet* out);
void Service_SelectByIdRange(const SeqList* L, const char* lo, const char* hi, IndexSet* out);
void Service_SelectByBirthRange(const SeqList* L, const Date* lo, const Date* hi, IndexSet* out);
void Service_SelectByAgeRange(const SeqList* L, int lo, int hi, IndexSet* out);
void Service_SelectByDept(const SeqList* L, Department dept, IndexSet* out);

/* ===================== 区间修改（对筛选结果批量更新） ===================== */
int Service_PromoteTitle(SeqList* L, const IndexSet* set);              /* 职称晋升一级，返回实际修改条数 */
int Service_PromoteEducation(SeqList* L, const IndexSet* set);          /* 学历提升一级(学位同步)，返回修改条数 */
int Service_ChangeDept(SeqList* L, const IndexSet* set, Department dept);/* 统一调整科室，返回修改条数 */

/* ===================== 区间统计 ===================== */
void Service_StatsSet(const SeqList* L, const IndexSet* set, Stats* out);
void Service_PrintStats(const Stats* s);

/* ===================== 排序 ===================== */
void Service_SortById(SeqList* L);
void Service_SortByName(SeqList* L);
void Service_SortByAge(SeqList* L);

/* ===================== 文件与测试数据 ===================== */
int Service_Save(const SeqList* L, const char* path);                   /* 1 成功；0 失败 */
int Service_Load(SeqList* L, const char* path, int* skipped);           /* 返回加载条数；-1 文件打不开 */
int Service_Generate(SeqList* L, int count);                            /* 返回实际新增条数 */

#endif /* SERVICE_H */
