/**
 * service.h —— 业务层
 *
 * 在顺序表之上封装：
 *   入职（自动派生工号 + 按序检索空位 + 重用离职工号）、
 *   离职（切换不外显标识符，后续工号不前移）、
 *   复合检索、区间查找 / 区间修改 / 区间统计、结果集排序、文件读写。
 */
#ifndef SERVICE_H
#define SERVICE_H

#include "doctor.h"
#include "seqlist.h"

/* 检索结果的最大条数 */
#define MAX_RESULT 1024

/* 检索结果集合：保存命中记录在顺序表中的栏位下标 */
typedef struct {
    int idx[MAX_RESULT];
    int count;
} IndexSet;

/* ===================== 复合检索条件 ===================== */
/* 每个 useXxx 为 1 表示该条件参与检索，多个条件之间是「与」关系 */
typedef struct {
    int        useWorkId;    char       workId[WORKID_LEN];  /* 工号精确 */
    int        useWorkIdRange;                            /* 工号区间 */
                             char       workIdLo[WORKID_LEN];
                             char       workIdHi[WORKID_LEN];
    int        useName;      char       name[NAME_LEN];      /* 姓名模糊 */
    int        useIdCard;    char       idCard[IDCARD_LEN];  /* 身份证号精确 */
    int        useGender;    Gender     gender;              /* 性别 */
    int        useAge;       int        ageLo, ageHi;        /* 年龄区间 */
    int        useBirth;     Date       birthLo, birthHi;    /* 出生日期区间 */
    int        useEdu;       Education  edu;                 /* 学历 */
    int        useDegree;    Degree     degree;              /* 学位 */
    int        useMajor;     char       major[STR_LEN];      /* 专业模糊 */
    int        usePosition;  Position   position;            /* 职务 */
    int        useTitle;     Title      title;               /* 职称 */
    int        useDept;      Department dept;                /* 科室 */
    int        useField;     Field      field;               /* 研究领域 */
    int        activeFilter;                                 /* -1 仅在职 / 0 全部 / 1 仅离职 */
} Query;

/* ===================== 入职结果 ===================== */
typedef enum {
    HIRE_FAIL_MEMORY     =  0,   /* 内存不足 */
    HIRE_OK_REUSE_WORKID =  1,   /* 重用了离职老员工的工号，落回其原栏位 */
    HIRE_OK_REUSE_SLOT   =  2,   /* 复用了最靠前的空位栏位（工号为新派生） */
    HIRE_OK_NEW_SLOT     =  3,   /* 人员真实扩充，末尾新开辟栏位 */
    HIRE_FAIL_DUP_WORKID = -1,   /* 派生出的工号已被在岗人员占用 */
    HIRE_FAIL_DUP_IDCARD = -2    /* 身份证号重复 */
} HireResult;

/* 排序键 */
typedef enum { SORT_BY_WORKID = 0, SORT_BY_NAME, SORT_BY_AGE } SortKey;

/* 统计结果 */
typedef struct {
    int count;
    int ageSum;
    int minAge;
    int maxAge;
    int genderCount[GENDER_COUNT];
    int eduCount[EDU_COUNT];
    int deptCount[DEPT_COUNT];
    int titleCount[TITLE_COUNT];
    int fieldCount[FIELD_COUNT];
} Stats;

/* ===================== 入职 / 离职 ===================== */
/* 工号由属性自动派生，不接受手工输入；outSlot 返回落位的栏位号（1 起） */
int  Service_Hire(SeqList* L, Doctor* d, int* outSlot);
/* 逻辑删除：把不外显标识符切为 SLOT_VACANT，后续工号不前移 */
int  Service_Resign(SeqList* L, const char* workId);
/* 物理清理所有空位栏位，返回清理数量（管理用途） */
int  Service_PurgeVacant(SeqList* L);

/* ===================== 计数 ===================== */
int  Service_ActiveCount(const SeqList* L);
int  Service_VacantCount(const SeqList* L);

/* ===================== 查找 ===================== */
int     Service_FindIndexByWorkId(const SeqList* L, const char* workId);   /* 仅在职，-1 未找到 */
int     Service_FindIndexByIdCard(const SeqList* L, const char* idCard);   /* 仅在职，-1 未找到 */
Doctor* Service_FindByWorkId(SeqList* L, const char* workId);              /* 仅在职 */

/* ===================== 复合检索 ===================== */
void Query_Init(Query* q);
void Service_Query(const SeqList* L, const Query* q, IndexSet* out);

/* ===================== 区间查找（内部即复合检索的简化入口） ===================== */
void Service_SelectAll(const SeqList* L, IndexSet* out);
void Service_SelectByWorkIdRange(const SeqList* L, const char* lo, const char* hi, IndexSet* out);
void Service_SelectByBirthRange(const SeqList* L, const Date* lo, const Date* hi, IndexSet* out);
void Service_SelectByAgeRange(const SeqList* L, int lo, int hi, IndexSet* out);
void Service_SelectByDept(const SeqList* L, Department dept, IndexSet* out);

/* ===================== 区间修改 ===================== */
/* 属性变更会影响工号，故修改后自动重算工号；若重算结果与在岗人员冲突则回滚该条 */
int Service_PromoteTitle(SeqList* L, const IndexSet* set, int* conflict);
int Service_PromoteEducation(SeqList* L, const IndexSet* set, int* conflict);
int Service_ChangeDept(SeqList* L, const IndexSet* set, Department dept, int* conflict);

/* 按当前属性重算指定栏位的工号；返回 0 表示与在岗人员冲突（已保持原值） */
int Service_RebuildWorkId(SeqList* L, int idx);

/* ===================== 区间统计 ===================== */
void Service_StatsSet(const SeqList* L, const IndexSet* set, Stats* out);
void Service_PrintStats(const Stats* s);

/* ===================== 排序 ===================== */
/* 只调整结果集的显示次序，不移动物理栏位，以免破坏空位复用语义 */
void Service_SortSet(const SeqList* L, IndexSet* set, SortKey key);

/* ===================== 文件与测试数据 ===================== */
int Service_Save(const SeqList* L, const char* path);                  /* 1 成功；0 失败 */
int Service_Load(SeqList* L, const char* path, int* skipped);          /* 返回加载条数；-1 文件打不开 */
int Service_Generate(SeqList* L, int count);                           /* 返回实际新增条数 */

#endif /* SERVICE_H */
