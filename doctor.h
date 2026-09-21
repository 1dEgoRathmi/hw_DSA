/**
 * doctor.h —— 数据层
 * 定义博士(医师)信息的结构体、日期工具、枚举与中文名称的映射、以及打印工具。
 */
#ifndef DOCTOR_H
#define DOCTOR_H

/* MSVC 下让源文件中的中文以 UTF-8 写入可执行文件，避免控制台乱码 */
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif

/* ===================== 字段长度常量 ===================== */
#define NAME_LEN 32              /* 姓名最大字节数 */
#define STR_LEN  64              /* 普通文本字段最大字节数 */
#define ID_LEN   19              /* 18 位工号 + 结束符 */

/* ===================== 枚举字段 ===================== */
typedef enum { GENDER_MALE = 0, GENDER_FEMALE, GENDER_COUNT } Gender;
typedef enum { EDU_COLLEGE = 0, EDU_BACHELOR, EDU_MASTER, EDU_PHD, EDU_POSTDOC, EDU_COUNT } Education;
typedef enum { DEG_NONE = 0, DEG_BACHELOR, DEG_MASTER, DEG_PHD, DEG_COUNT } Degree;
typedef enum { POS_DOCTOR = 0, POS_VICE_DIRECTOR, POS_DIRECTOR, POS_VICE_PRESIDENT, POS_PRESIDENT, POS_COUNT } Position;
typedef enum { TITLE_RESIDENT = 0, TITLE_ATTENDING, TITLE_ASSOC_CHIEF, TITLE_CHIEF, TITLE_COUNT } Title;
typedef enum { DEPT_INTERNAL = 0, DEPT_SURGERY, DEPT_OBGYN, DEPT_PEDIATRICS, DEPT_EMERGENCY, DEPT_IMAGING, DEPT_COUNT } Department;
typedef enum { FIELD_CLINICAL = 0, FIELD_RESEARCH, FIELD_TEACHING, FIELD_MANAGEMENT, FIELD_COUNT } Field;

/* ===================== 数据实体 ===================== */
typedef struct {
    int year;
    int month;
    int day;
} Date;

typedef struct {
    char       name[NAME_LEN];   /* 姓名 */
    char       id[ID_LEN];       /* 工号，18 位数字 */
    Gender     gender;           /* 性别 */
    Date       birth;            /* 出生日期 */
    Education  education;        /* 学历 */
    Degree     degree;           /* 学位 */
    char       major[STR_LEN];   /* 专业 */
    Position   position;         /* 职务 */
    Title      title;            /* 职称 */
    Department dept;             /* 科室 */
    Field      field;            /* 研究领域 */
} Doctor;

/* 枚举值 -> 中文名称表，下标即枚举值 */
extern const char* const GENDER_NAMES[GENDER_COUNT];
extern const char* const EDU_NAMES[EDU_COUNT];
extern const char* const DEG_NAMES[DEG_COUNT];
extern const char* const POS_NAMES[POS_COUNT];
extern const char* const TITLE_NAMES[TITLE_COUNT];
extern const char* const DEPT_NAMES[DEPT_COUNT];
extern const char* const FIELD_NAMES[FIELD_COUNT];

/* ===================== 日期工具 ===================== */
int  Date_IsLeap(int year);                                  /* 是否闰年 */
int  Date_IsValid(int year, int month, int day);             /* 日期是否合法(1900-2100) */
void Date_ToStr(const Date* d, char* buf, int size);         /* 转 "YYYY-MM-DD" */
int  Date_FromStr(const char* text, Date* out);              /* 解析 "YYYY-MM-DD" / "YYYY/MM/DD" */
int  Date_Compare(const Date* a, const Date* b);             /* a<b 返回 -1，相等 0，a>b 返回 1 */
int  Date_AgeOn(const Date* birth, const Date* today);       /* 指定日期当天的周岁 */
const Date* Date_Today(void);                                /* 当天日期(进程内缓存) */
int  Date_Age(const Date* birth);                            /* 相对今天的周岁 */

/* ===================== 枚举工具 ===================== */
const char* Enum_Name(const char* const* table, int count, int value);          /* 枚举值 -> 中文 */
int Enum_Parse(const char* const* table, int count, const char* text, int* out);/* 中文 -> 枚举值 */

/* ===================== 文本对齐工具 ===================== */
int  Text_DisplayWidth(const char* s);        /* 按终端显示宽度计算(中文按 2 列) */
void Text_PrintPad(const char* s, int width); /* 输出 s 并补空格到指定显示宽度 */

/* ===================== Doctor 操作 ===================== */
int  Doctor_SetId(Doctor* d, const char* id);                          /* 校验并写入 18 位工号 */
int  Doctor_Validate(const Doctor* d, char* err, int errSize);         /* 全字段校验 */
int  Doctor_CompareId(const Doctor* a, const Doctor* b);               /* 按工号比较 */

void Doctor_PrintHeader(void);                    /* 表格表头 */
void Doctor_PrintRow(int seq, const Doctor* d);   /* 表格单行(概要字段) */
void Doctor_PrintDetail(const Doctor* d);         /* 单条记录完整信息 */

void Doctor_Random(Doctor* d, int seq);           /* 生成一条随机数据，seq 用于构造唯一工号 */

#endif /* DOCTOR_H */
