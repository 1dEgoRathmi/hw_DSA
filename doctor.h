/**
 * doctor.h —— 数据层
 *
 * v2.1 数据模型要点：
 *   1. 工号与身份证号彻底分离：工号是 8 位结构化内部编号，身份证号是 18 位真实身份标识。
 *   2. 8 位工号 = 科室(1) + 研究领域(1) + 职称(1) + 姓(2) + 名(3)，由属性自动派生，不接受手工输入。
 *   3. active 为“不外显标识符”，只用于内部控制工号是否被占用，不出现在任何人员信息表中。
 */
#ifndef DOCTOR_H
#define DOCTOR_H

/* MSVC 下让源文件中的中文以 UTF-8 写入可执行文件，避免控制台乱码 */
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif

/* ===================== 字段长度常量 ===================== */
#define NAME_LEN   32               /* 姓名最大字节数 */
#define STR_LEN    64               /* 普通文本字段最大字节数 */
#define WORKID_LEN 9                /* 8 位工号 + 结束符 */
#define IDCARD_LEN 19               /* 18 位身份证号 + 结束符 */

/* ===================== 8 位工号的分段规则 ===================== */
#define WORKID_DIGITS        8      /* 工号总位数 */
#define WORKID_SEG_DEPT      1      /* 第 1 位：科室 */
#define WORKID_SEG_FIELD     1      /* 第 2 位：研究领域 */
#define WORKID_SEG_TITLE     1      /* 第 3 位：职称 */
#define WORKID_SEG_SURNAME   2      /* 第 4-5 位：姓（姓氏表序号） */
#define WORKID_SEG_GIVEN     3      /* 第 6-8 位：名（名称摘要码） */

/* ===================== 不外显标识符取值 ===================== */
#define SLOT_VACANT 0               /* 空位：原人员已离职，工号可被重用 */
#define SLOT_INUSE  1               /* 在岗：工号被当前人员占用 */

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
    char       workId[WORKID_LEN];   /* 8 位结构化工号，由属性派生 */
    char       idCard[IDCARD_LEN];   /* 18 位身份证号 */
    int        active;               /* 不外显标识符：SLOT_INUSE / SLOT_VACANT */
    char       name[NAME_LEN];       /* 姓名 */
    Gender     gender;               /* 性别，由身份证号派生 */
    Date       birth;                /* 出生日期，由身份证号派生 */
    Education  education;            /* 学历 */
    Degree     degree;               /* 学位 */
    char       major[STR_LEN];       /* 专业 */
    Position   position;             /* 职务 */
    Title      title;                /* 职称 */
    Department dept;                 /* 科室 */
    Field      field;                /* 研究领域 */
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
int  Date_IsLeap(int year);
int  Date_IsValid(int year, int month, int day);
void Date_ToStr(const Date* d, char* buf, int size);
int  Date_FromStr(const char* text, Date* out);
int  Date_Compare(const Date* a, const Date* b);
int  Date_AgeOn(const Date* birth, const Date* today);
const Date* Date_Today(void);
int  Date_Age(const Date* birth);

/* ===================== 枚举工具 ===================== */
const char* Enum_Name(const char* const* table, int count, int value);
int Enum_Parse(const char* const* table, int count, const char* text, int* out);

/* ===================== 文本对齐工具 ===================== */
int  Text_DisplayWidth(const char* s);
void Text_PrintPad(const char* s, int width);

/* ===================== 工号 ===================== */
/* 依据 科室+领域+职称+姓+名 派生 8 位工号，写入 d->workId */
int  Doctor_BuildWorkId(Doctor* d);
/* 校验 8 位工号格式 */
int  Doctor_WorkIdValid(const char* workId);
/* 把工号拆解为各分段的数值，便于讲解与展示；任一参数可为 NULL */
void Doctor_ExplainWorkId(const char* workId, int* dept, int* field, int* title,
                          int* surnameCode, int* givenCode);

/* ===================== 身份证号 ===================== */
/* 校验 18 位身份证号（含校验位），并派生 性别 / 出生日期 */
int  Doctor_SetIdCard(Doctor* d, const char* idCard);
/* 校验身份证号，返回 1 合法；否则返回 0 并写入错误原因 */
int  IdCard_Validate(const char* idCard, char* err, int errSize);
/* 生成一个校验位正确的身份证号，seq 仅用于制造差异 */
void IdCard_Make(char* out, const Date* birth, Gender gender, int seq);

/* ===================== Doctor 操作 ===================== */
int  Doctor_Validate(const Doctor* d, char* err, int errSize);

void Doctor_PrintHeaderBrief(void);
void Doctor_PrintRowBrief(int seq, const Doctor* d);
void Doctor_PrintHeaderFull(void);
void Doctor_PrintRowFull(int seq, const Doctor* d);
void Doctor_PrintDetail(const Doctor* d);

void Doctor_Random(Doctor* d, int seq);

#endif /* DOCTOR_H */
