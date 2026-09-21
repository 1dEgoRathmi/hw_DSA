/**
 * doctor.c —— 数据层实现
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "doctor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ===================== 枚举名称表 ===================== */
const char* const GENDER_NAMES[GENDER_COUNT] = { "男", "女" };
const char* const EDU_NAMES[EDU_COUNT]       = { "大专", "本科", "硕士", "博士", "博士后" };
const char* const DEG_NAMES[DEG_COUNT]       = { "无", "学士", "硕士", "博士" };
const char* const POS_NAMES[POS_COUNT]       = { "医师", "副主任", "主任", "副院长", "院长" };
const char* const TITLE_NAMES[TITLE_COUNT]   = { "住院医师", "主治医师", "副主任医师", "主任医师" };
const char* const DEPT_NAMES[DEPT_COUNT]     = { "内科", "外科", "妇产科", "儿科", "急诊科", "影像科" };
const char* const FIELD_NAMES[FIELD_COUNT]   = { "临床", "科研", "教学", "管理" };

/* ===================== 日期工具 ===================== */
int Date_IsLeap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int Date_IsValid(int year, int month, int day)
{
    static const int days[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int limit;

    if (year < 1900 || year > 2100) return 0;
    if (month < 1 || month > 12)    return 0;

    limit = days[month] + ((month == 2 && Date_IsLeap(year)) ? 1 : 0);
    return day >= 1 && day <= limit;
}

void Date_ToStr(const Date* d, char* buf, int size)
{
    snprintf(buf, (size_t)size, "%04d-%02d-%02d", d->year, d->month, d->day);
}

int Date_FromStr(const char* text, Date* out)
{
    int y = 0, m = 0, d = 0;

    if (!text || !out) return 0;
    if (sscanf(text, "%d-%d-%d", &y, &m, &d) != 3 &&
        sscanf(text, "%d/%d/%d", &y, &m, &d) != 3) {
        return 0;
    }
    if (!Date_IsValid(y, m, d)) return 0;

    out->year = y;
    out->month = m;
    out->day = d;
    return 1;
}

int Date_Compare(const Date* a, const Date* b)
{
    if (a->year  != b->year)  return a->year  < b->year  ? -1 : 1;
    if (a->month != b->month) return a->month < b->month ? -1 : 1;
    if (a->day   != b->day)   return a->day   < b->day   ? -1 : 1;
    return 0;
}

int Date_AgeOn(const Date* birth, const Date* today)
{
    int age = today->year - birth->year;

    if (today->month < birth->month ||
        (today->month == birth->month && today->day < birth->day)) {
        age--;
    }
    return age < 0 ? 0 : age;
}

const Date* Date_Today(void)
{
    static Date today;
    static int  ready = 0;

    if (!ready) {
        time_t t = time(NULL);
        struct tm* lt = localtime(&t);
        if (lt) {
            today.year  = lt->tm_year + 1900;
            today.month = lt->tm_mon + 1;
            today.day   = lt->tm_mday;
        } else {
            today.year = 2000; today.month = 1; today.day = 1;
        }
        ready = 1;
    }
    return &today;
}

int Date_Age(const Date* birth)
{
    return Date_AgeOn(birth, Date_Today());
}

/* ===================== 枚举工具 ===================== */
const char* Enum_Name(const char* const* table, int count, int value)
{
    if (!table || value < 0 || value >= count) return "未知";
    return table[value];
}

int Enum_Parse(const char* const* table, int count, const char* text, int* out)
{
    int i;

    if (!table || !text || !out) return 0;
    for (i = 0; i < count; i++) {
        if (strcmp(table[i], text) == 0) {
            *out = i;
            return 1;
        }
    }
    return 0;
}

/* ===================== 文本对齐工具 ===================== */
int Text_DisplayWidth(const char* s)
{
    int width = 0;
    const unsigned char* p = (const unsigned char*)s;

    if (!s) return 0;
    while (*p) {
        if (*p < 0x80) {                      /* ASCII */
            width += 1;
            p += 1;
        } else if ((*p & 0xE0) == 0xC0) {     /* UTF-8 双字节 */
            width += 2;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {     /* UTF-8 三字节(常用汉字) */
            width += 2;
            p += 3;
        } else if ((*p & 0xF8) == 0xF0) {     /* UTF-8 四字节 */
            width += 2;
            p += 4;
        } else {                              /* GBK 等双字节编码的次字节 */
            width += 1;
            p += 1;
        }
    }
    return width;
}

void Text_PrintPad(const char* s, int width)
{
    int i;
    int used = Text_DisplayWidth(s);

    fputs(s ? s : "", stdout);
    for (i = used; i < width; i++) putchar(' ');
}

/* ===================== Doctor 操作 ===================== */
int Doctor_SetId(Doctor* d, const char* id)
{
    size_t i;
    size_t len;

    if (!d || !id) return 0;
    len = strlen(id);
    if (len != 18) return 0;
    for (i = 0; i < len; i++) {
        if (!isdigit((unsigned char)id[i])) return 0;
    }
    memcpy(d->id, id, ID_LEN);
    return 1;
}

int Doctor_Validate(const Doctor* d, char* err, int errSize)
{
    size_t i;

    if (!d) {
        if (err && errSize > 0) snprintf(err, (size_t)errSize, "记录为空");
        return 0;
    }
#define FAIL_MSG(msg) do { if (err && errSize > 0) snprintf(err, (size_t)errSize, "%s", msg); return 0; } while (0)

    if (d->name[0] == '\0')                       FAIL_MSG("姓名不能为空");
    if (strlen(d->id) != 18)                      FAIL_MSG("工号必须为 18 位");
    for (i = 0; i < 18; i++) {
        if (!isdigit((unsigned char)d->id[i]))    FAIL_MSG("工号必须全部为数字");
    }
    if (d->gender < 0 || d->gender >= GENDER_COUNT)          FAIL_MSG("性别取值越界");
    if (!Date_IsValid(d->birth.year, d->birth.month, d->birth.day))
                                                             FAIL_MSG("出生日期不合法");
    if (d->education < 0 || d->education >= EDU_COUNT)       FAIL_MSG("学历取值越界");
    if (d->degree < 0 || d->degree >= DEG_COUNT)             FAIL_MSG("学位取值越界");
    if (d->major[0] == '\0')                                 FAIL_MSG("专业不能为空");
    if (d->position < 0 || d->position >= POS_COUNT)         FAIL_MSG("职务取值越界");
    if (d->title < 0 || d->title >= TITLE_COUNT)             FAIL_MSG("职称取值越界");
    if (d->dept < 0 || d->dept >= DEPT_COUNT)                FAIL_MSG("科室取值越界");
    if (d->field < 0 || d->field >= FIELD_COUNT)             FAIL_MSG("研究领域取值越界");

#undef FAIL_MSG
    return 1;
}

int Doctor_CompareId(const Doctor* a, const Doctor* b)
{
    return strcmp(a->id, b->id);
}

/* ===================== 打印 ===================== */
#define TABLE_WIDTH 91

void Doctor_PrintHeader(void)
{
    int i;

    Text_PrintPad("序号",     4);
    Text_PrintPad("工号",     19);
    Text_PrintPad("姓名",     10);
    Text_PrintPad("性别",     6);
    Text_PrintPad("年龄",     6);
    Text_PrintPad("学历",     8);
    Text_PrintPad("学位",     8);
    Text_PrintPad("职称",     12);
    Text_PrintPad("科室",     8);
    Text_PrintPad("研究领域", 10);
    printf("\n");
    for (i = 0; i < TABLE_WIDTH; i++) putchar('-');
    printf("\n");
}

void Doctor_PrintRow(int seq, const Doctor* d)
{
    char num[16];
    char age[16];

    if (!d) return;

    snprintf(num, sizeof num, "%d", seq);
    snprintf(age, sizeof age, "%d", Date_Age(&d->birth));

    Text_PrintPad(num, 4);
    Text_PrintPad(d->id, 19);
    Text_PrintPad(d->name, 10);
    Text_PrintPad(Enum_Name(GENDER_NAMES, GENDER_COUNT, d->gender), 6);
    Text_PrintPad(age, 6);
    Text_PrintPad(Enum_Name(EDU_NAMES, EDU_COUNT, d->education), 8);
    Text_PrintPad(Enum_Name(DEG_NAMES, DEG_COUNT, d->degree), 8);
    Text_PrintPad(Enum_Name(TITLE_NAMES, TITLE_COUNT, d->title), 12);
    Text_PrintPad(Enum_Name(DEPT_NAMES, DEPT_COUNT, d->dept), 8);
    Text_PrintPad(Enum_Name(FIELD_NAMES, FIELD_COUNT, d->field), 10);
    printf("\n");
}

void Doctor_PrintDetail(const Doctor* d)
{
    char birth[16];

    if (!d) return;
    Date_ToStr(&d->birth, birth, sizeof birth);

    printf("--------------------------------------------------\n");
    printf("  姓名      : %s\n", d->name);
    printf("  工号      : %s\n", d->id);
    printf("  性别      : %s\n", Enum_Name(GENDER_NAMES, GENDER_COUNT, d->gender));
    printf("  出生日期  : %s (%d 岁)\n", birth, Date_Age(&d->birth));
    printf("  学历      : %s\n", Enum_Name(EDU_NAMES, EDU_COUNT, d->education));
    printf("  学位      : %s\n", Enum_Name(DEG_NAMES, DEG_COUNT, d->degree));
    printf("  专业      : %s\n", d->major);
    printf("  职务      : %s\n", Enum_Name(POS_NAMES, POS_COUNT, d->position));
    printf("  职称      : %s\n", Enum_Name(TITLE_NAMES, TITLE_COUNT, d->title));
    printf("  科室      : %s\n", Enum_Name(DEPT_NAMES, DEPT_COUNT, d->dept));
    printf("  研究领域  : %s\n", Enum_Name(FIELD_NAMES, FIELD_COUNT, d->field));
    printf("--------------------------------------------------\n");
}

/* ===================== 随机数据生成 ===================== */
#define ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const char* const RND_SURNAMES[] = {
    "王", "李", "张", "刘", "陈", "杨", "赵", "黄", "周", "吴",
    "徐", "孙", "马", "朱", "胡", "郭", "何", "林", "高", "罗",
    "郑", "梁", "谢", "宋", "唐", "许", "韩", "冯", "邓", "曹"
};

static const char* const RND_GIVEN1[] = {
    "建", "志", "文", "国", "晓", "雅", "明", "海", "雪", "慧",
    "俊", "淑", "丽", "永", "秀", "世", "兰", "振", "春", "梦",
    "嘉", "宇", "思", "子", "宏", "立", "安", "瑞", "启", "望"
};

static const char* const RND_GIVEN2[] = {
    "华", "军", "平", "强", "敏", "静", "丽", "杰", "娜", "峰",
    "涛", "燕", "磊", "霞", "波", "芳", "刚", "梅", "勇", "倩",
    "宁", "阳", "琳", "楠", "辰", "轩", "然", "菲", "维", "哲"
};

static const char* const RND_MAJORS[] = {
    "心血管病学", "消化系病学", "呼吸系病学", "内分泌与代谢病学",
    "普通外科学", "骨科学", "神经外科学", "泌尿外科学",
    "妇产科学", "儿科学", "急诊医学", "影像医学与核医学",
    "肿瘤学", "麻醉学", "神经病学", "皮肤病与性病学"
};

void Doctor_Random(Doctor* d, int seq)
{
    int age;
    int roll;

    if (!d) return;
    memset(d, 0, sizeof *d);

    /* 姓名：姓 + 1~2 个名字 */
    if (rand() % 2) {
        snprintf(d->name, NAME_LEN, "%s%s%s",
                 RND_SURNAMES[rand() % ARRAY_COUNT(RND_SURNAMES)],
                 RND_GIVEN1[rand() % ARRAY_COUNT(RND_GIVEN1)],
                 RND_GIVEN2[rand() % ARRAY_COUNT(RND_GIVEN2)]);
    } else {
        snprintf(d->name, NAME_LEN, "%s%s",
                 RND_SURNAMES[rand() % ARRAY_COUNT(RND_SURNAMES)],
                 RND_GIVEN1[rand() % ARRAY_COUNT(RND_GIVEN1)]);
    }

    /* 工号：4 位年份 + 14 位流水号，恰好 18 位且互不相同 */
    snprintf(d->id, ID_LEN, "%04d%014d", 1990 + rand() % 36, seq);

    d->gender = (Gender)(rand() % GENDER_COUNT);

    /* 出生日期：1960 ~ 2000 年，日取 1~28 规避大小月问题 */
    d->birth.year  = 1960 + rand() % 41;
    d->birth.month = 1 + rand() % 12;
    d->birth.day   = 1 + rand() % 28;

    age = Date_Age(&d->birth);

    /* 学历、学位、职称按年龄梯度分布，使数据更贴近真实 */
    if (age >= 45) {
        d->education = (rand() % 10 < 7) ? EDU_PHD : EDU_POSTDOC;
    } else if (age >= 35) {
        d->education = EDU_PHD;
    } else if (age >= 30) {
        d->education = (rand() % 2) ? EDU_MASTER : EDU_PHD;
    } else {
        d->education = (rand() % 2) ? EDU_BACHELOR : EDU_MASTER;
    }

    switch (d->education) {
        case EDU_COLLEGE:  d->degree = DEG_NONE;     break;
        case EDU_BACHELOR: d->degree = DEG_BACHELOR; break;
        case EDU_MASTER:   d->degree = DEG_MASTER;   break;
        default:           d->degree = DEG_PHD;      break;
    }

    if (age < 30)      d->title = TITLE_RESIDENT;
    else if (age < 38) d->title = TITLE_ATTENDING;
    else if (age < 48) d->title = TITLE_ASSOC_CHIEF;
    else               d->title = TITLE_CHIEF;

    roll = rand() % 100;
    if (roll < 70)      d->position = POS_DOCTOR;
    else if (roll < 88) d->position = POS_VICE_DIRECTOR;
    else if (roll < 96) d->position = POS_DIRECTOR;
    else if (roll < 99) d->position = POS_VICE_PRESIDENT;
    else                d->position = POS_PRESIDENT;

    snprintf(d->major, STR_LEN, "%s", RND_MAJORS[rand() % ARRAY_COUNT(RND_MAJORS)]);

    d->dept  = (Department)(rand() % DEPT_COUNT);
    d->field = (Field)(rand() % FIELD_COUNT);
}
