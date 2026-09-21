/**
 * doctor.c —— 数据层实现
 *
 * 负责：日期工具、枚举映射、文本宽度对齐、8 位结构化工号的生成与解析、
 *       身份证号的校验与派生、两种模式的表格输出、随机测试数据生成。
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

/* ===================== 8 位结构化工号 ===================== */
/* 姓氏表：姓 -> 2 位编码（表内序号，1 起）。表外姓氏走哈希回退 */
static const char* const SURNAME_TABLE[] = {
    "王", "李", "张", "刘", "陈", "杨", "黄", "赵", "吴", "周",
    "徐", "孙", "马", "朱", "胡", "郭", "何", "高", "林", "罗",
    "郑", "梁", "谢", "宋", "唐", "许", "韩", "冯", "邓", "曹",
    "彭", "曾", "肖", "田", "董", "袁", "潘", "于", "蒋", "蔡",
    "余", "杜", "叶", "程", "苏", "魏", "吕", "丁", "任", "沈",
    "姚", "卢", "姜", "崔", "钟", "谭", "陆", "汪", "范", "金"
};
#define SURNAME_COUNT ((int)(sizeof(SURNAME_TABLE) / sizeof((SURNAME_TABLE)[0])))

static int Utf8_CharLen(unsigned char c)
{
    if (c < 0x80)           return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/* 把姓名拆成「姓」和「名」：首个 UTF-8 字符为姓，其余为名 */
static void Name_Split(const char* name, char* surname, int sSize, char* given, int gSize)
{
    int len;
    const unsigned char* p = (const unsigned char*)name;

    surname[0] = '\0';
    given[0]   = '\0';
    if (!name || !*name) return;

    len = Utf8_CharLen(*p);
    if (len >= sSize) len = sSize - 1;

    memcpy(surname, name, (size_t)len);
    surname[len] = '\0';
    snprintf(given, (size_t)gSize, "%s", name + len);
}

static int Name_Hash(const char* s, int mod)
{
    unsigned int h = 5381u;
    const unsigned char* p = (const unsigned char*)s;

    while (*p) {
        h = h * 33u + (unsigned int)(*p);
        p++;
    }
    return (int)(h % (unsigned int)mod);
}

static int Surname_Code(const char* surname)
{
    int i;

    for (i = 0; i < SURNAME_COUNT; i++) {
        if (strcmp(SURNAME_TABLE[i], surname) == 0) return i + 1;   /* 1 ~ 60 */
    }
    return 1 + Name_Hash(surname, 99);                              /* 1 ~ 99 回退 */
}

static int GivenName_Code(const char* given)
{
    if (given[0] == '\0') return 1;
    return 1 + Name_Hash(given, 999);                               /* 1 ~ 999 */
}

int Doctor_BuildWorkId(Doctor* d)
{
    char surname[8];
    char given[NAME_LEN];
    int  sc;
    int  gc;

    if (!d) return 0;

    Name_Split(d->name, surname, (int)sizeof surname, given, (int)sizeof given);
    sc = Surname_Code(surname);
    gc = GivenName_Code(given);

    /* 科室(1) 领域(1) 职称(1) 姓(2) 名(3) = 8 位 */
    snprintf(d->workId, WORKID_LEN, "%d%d%d%02d%03d",
             (int)d->dept, (int)d->field, (int)d->title, sc, gc);
    return 1;
}

int Doctor_WorkIdValid(const char* workId)
{
    int i;

    if (!workId || strlen(workId) != WORKID_DIGITS) return 0;
    for (i = 0; i < WORKID_DIGITS; i++) {
        if (!isdigit((unsigned char)workId[i])) return 0;
    }
    return 1;
}

void Doctor_ExplainWorkId(const char* workId, int* dept, int* field, int* title,
                          int* surnameCode, int* givenCode)
{
    char seg[4];

    if (!Doctor_WorkIdValid(workId)) return;

    if (dept)  *dept  = workId[0] - '0';
    if (field) *field = workId[1] - '0';
    if (title) *title = workId[2] - '0';

    if (surnameCode) {
        seg[0] = workId[3]; seg[1] = workId[4]; seg[2] = '\0';
        *surnameCode = atoi(seg);
    }
    if (givenCode) {
        seg[0] = workId[5]; seg[1] = workId[6]; seg[2] = workId[7]; seg[3] = '\0';
        *givenCode = atoi(seg);
    }
}

/* ===================== 身份证号 ===================== */
static const int  IDCARD_WEIGHT[17] = { 7, 9, 10, 5, 8, 4, 2, 1, 6, 3, 7, 9, 10, 5, 8, 4, 2 };
static const char IDCARD_CHECK[11]  = { '1', '0', 'X', '9', '8', '7', '6', '5', '4', '3', '2' };

int IdCard_Validate(const char* idCard, char* err, int errSize)
{
    int  i;
    int  sum = 0;
    int  year, month, day;
    char expect;

#define IDCARD_FAIL(msg) do { if (err && errSize > 0) snprintf(err, (size_t)errSize, "%s", msg); return 0; } while (0)

    if (!idCard || strlen(idCard) != 18)                    IDCARD_FAIL("身份证号必须为 18 位");

    for (i = 0; i < 17; i++) {
        if (!isdigit((unsigned char)idCard[i]))             IDCARD_FAIL("身份证号前 17 位必须为数字");
    }
    if (!isdigit((unsigned char)idCard[17]) &&
        toupper((unsigned char)idCard[17]) != 'X')          IDCARD_FAIL("身份证号校验位只能是数字或 X");

    year  = (idCard[6] - '0') * 1000 + (idCard[7] - '0') * 100 +
            (idCard[8] - '0') * 10 + (idCard[9] - '0');
    month = (idCard[10] - '0') * 10 + (idCard[11] - '0');
    day   = (idCard[12] - '0') * 10 + (idCard[13] - '0');
    if (!Date_IsValid(year, month, day))                    IDCARD_FAIL("身份证号中的出生日期不合法");

    for (i = 0; i < 17; i++) sum += (idCard[i] - '0') * IDCARD_WEIGHT[i];
    expect = IDCARD_CHECK[sum % 11];
    if (toupper((unsigned char)idCard[17]) != expect) {
        if (err && errSize > 0) snprintf(err, (size_t)errSize, "身份证号校验位错误，正确校验位应为 %c", expect);
        return 0;
    }

#undef IDCARD_FAIL
    return 1;
}

int Doctor_SetIdCard(Doctor* d, const char* idCard)
{
    int year, month, day;

    if (!d || !IdCard_Validate(idCard, NULL, 0)) return 0;

    memcpy(d->idCard, idCard, IDCARD_LEN);
    d->idCard[17] = (char)toupper((unsigned char)d->idCard[17]);

    year  = (idCard[6] - '0') * 1000 + (idCard[7] - '0') * 100 +
            (idCard[8] - '0') * 10 + (idCard[9] - '0');
    month = (idCard[10] - '0') * 10 + (idCard[11] - '0');
    day   = (idCard[12] - '0') * 10 + (idCard[13] - '0');

    d->birth.year  = year;
    d->birth.month = month;
    d->birth.day   = day;
    /* 第 17 位为顺序码末位，奇数为男、偶数为女 */
    d->gender = ((idCard[16] - '0') % 2) ? GENDER_MALE : GENDER_FEMALE;
    return 1;
}

void IdCard_Make(char* out, const Date* birth, Gender gender, int seq)
{
    int  i;
    int  sum = 0;
    int  area = 110000 + (seq * 37) % 90000;    /* 6 位地区码，仅作示意 */
    int  order = 100 + (seq * 13) % 900;        /* 3 位顺序码 */
    char body[19];

    /* 顺序码末位奇男偶女，保证与性别字段一致 */
    if (gender == GENDER_MALE) {
        if (order % 2 == 0) order++;
    } else {
        if (order % 2 == 1) order++;
    }

    snprintf(body, sizeof body, "%06d%04d%02d%02d%03d",
             area, birth->year, birth->month, birth->day, order);

    for (i = 0; i < 17; i++) sum += (body[i] - '0') * IDCARD_WEIGHT[i];
    /* body 已含 17 位数字，直接拼接校验码，避免格式化截断 */
    memcpy(out, body, 17);
    out[17] = IDCARD_CHECK[sum % 11];
    out[18] = '\0';
}

/* ===================== Doctor 校验 ===================== */
int Doctor_Validate(const Doctor* d, char* err, int errSize)
{
    Doctor derived;

#define DOCTOR_FAIL(msg) do { if (err && errSize > 0) snprintf(err, (size_t)errSize, "%s", msg); return 0; } while (0)

    if (!d)                                  DOCTOR_FAIL("记录为空");
    if (d->name[0] == '\0')                  DOCTOR_FAIL("姓名不能为空");
    if (!Doctor_WorkIdValid(d->workId))      DOCTOR_FAIL("工号必须为 8 位数字");
    if (!IdCard_Validate(d->idCard, err, errSize)) return 0;
    if (d->active != SLOT_INUSE && d->active != SLOT_VACANT)
                                             DOCTOR_FAIL("不外显标识符取值非法");
    if (d->gender < 0 || d->gender >= GENDER_COUNT)       DOCTOR_FAIL("性别取值越界");
    if (!Date_IsValid(d->birth.year, d->birth.month, d->birth.day))
                                                          DOCTOR_FAIL("出生日期不合法");
    if (d->education < 0 || d->education >= EDU_COUNT)    DOCTOR_FAIL("学历取值越界");
    if (d->degree < 0 || d->degree >= DEG_COUNT)          DOCTOR_FAIL("学位取值越界");
    if (d->major[0] == '\0')                              DOCTOR_FAIL("专业不能为空");
    if (d->position < 0 || d->position >= POS_COUNT)      DOCTOR_FAIL("职务取值越界");
    if (d->title < 0 || d->title >= TITLE_COUNT)          DOCTOR_FAIL("职称取值越界");
    if (d->dept < 0 || d->dept >= DEPT_COUNT)             DOCTOR_FAIL("科室取值越界");
    if (d->field < 0 || d->field >= FIELD_COUNT)          DOCTOR_FAIL("研究领域取值越界");

    /* 校验工号与属性的一致性：工号必须由 科室+领域+职称+姓名 派生 */
    derived = *d;
    Doctor_BuildWorkId(&derived);
    if (strcmp(derived.workId, d->workId) != 0) {
        if (err && errSize > 0) {
            snprintf(err, (size_t)errSize,
                     "工号 %s 与科室/领域/职称/姓名不匹配，应为 %s", d->workId, derived.workId);
        }
        return 0;
    }

#undef DOCTOR_FAIL
    return 1;
}

/* ===================== 表格输出 ===================== */
#define TABLE_WIDTH_BRIEF 76
#define TABLE_WIDTH_FULL  141

void Doctor_PrintHeaderBrief(void)
{
    int i;

    Text_PrintPad("序号",     4);
    Text_PrintPad("工号",     10);
    Text_PrintPad("姓名",     10);
    Text_PrintPad("性别",     6);
    Text_PrintPad("年龄",     6);
    Text_PrintPad("职务",     10);
    Text_PrintPad("职称",     12);
    Text_PrintPad("科室",     8);
    Text_PrintPad("研究领域", 10);
    printf("\n");
    for (i = 0; i < TABLE_WIDTH_BRIEF; i++) putchar('-');
    printf("\n");
}

void Doctor_PrintRowBrief(int seq, const Doctor* d)
{
    char num[16];
    char age[16];

    if (!d) return;

    snprintf(num, sizeof num, "%d", seq);
    snprintf(age, sizeof age, "%d", Date_Age(&d->birth));

    Text_PrintPad(num, 4);
    Text_PrintPad(d->workId, 10);
    Text_PrintPad(d->name, 10);
    Text_PrintPad(Enum_Name(GENDER_NAMES, GENDER_COUNT, d->gender), 6);
    Text_PrintPad(age, 6);
    Text_PrintPad(Enum_Name(POS_NAMES, POS_COUNT, d->position), 10);
    Text_PrintPad(Enum_Name(TITLE_NAMES, TITLE_COUNT, d->title), 12);
    Text_PrintPad(Enum_Name(DEPT_NAMES, DEPT_COUNT, d->dept), 8);
    Text_PrintPad(Enum_Name(FIELD_NAMES, FIELD_COUNT, d->field), 10);
    printf("\n");
}

void Doctor_PrintHeaderFull(void)
{
    int i;

    Text_PrintPad("序号",     4);
    Text_PrintPad("工号",     10);
    Text_PrintPad("身份证号", 19);
    Text_PrintPad("姓名",     10);
    Text_PrintPad("性别",     6);
    Text_PrintPad("出生日期", 12);
    Text_PrintPad("年龄",     6);
    Text_PrintPad("学历",     8);
    Text_PrintPad("学位",     8);
    Text_PrintPad("专业",     18);
    Text_PrintPad("职务",     10);
    Text_PrintPad("职称",     12);
    Text_PrintPad("科室",     8);
    Text_PrintPad("研究领域", 10);
    printf("\n");
    for (i = 0; i < TABLE_WIDTH_FULL; i++) putchar('-');
    printf("\n");
}

void Doctor_PrintRowFull(int seq, const Doctor* d)
{
    char num[16];
    char birth[16];
    char age[16];

    if (!d) return;

    snprintf(num, sizeof num, "%d", seq);
    snprintf(age, sizeof age, "%d", Date_Age(&d->birth));
    Date_ToStr(&d->birth, birth, sizeof birth);

    Text_PrintPad(num, 4);
    Text_PrintPad(d->workId, 10);
    Text_PrintPad(d->idCard, 19);
    Text_PrintPad(d->name, 10);
    Text_PrintPad(Enum_Name(GENDER_NAMES, GENDER_COUNT, d->gender), 6);
    Text_PrintPad(birth, 12);
    Text_PrintPad(age, 6);
    Text_PrintPad(Enum_Name(EDU_NAMES, EDU_COUNT, d->education), 8);
    Text_PrintPad(Enum_Name(DEG_NAMES, DEG_COUNT, d->degree), 8);
    Text_PrintPad(d->major, 18);
    Text_PrintPad(Enum_Name(POS_NAMES, POS_COUNT, d->position), 10);
    Text_PrintPad(Enum_Name(TITLE_NAMES, TITLE_COUNT, d->title), 12);
    Text_PrintPad(Enum_Name(DEPT_NAMES, DEPT_COUNT, d->dept), 8);
    Text_PrintPad(Enum_Name(FIELD_NAMES, FIELD_COUNT, d->field), 10);
    printf("\n");
}

void Doctor_PrintDetail(const Doctor* d)
{
    char birth[16];
    int  dept = 0, field = 0, title = 0, sc = 0, gc = 0;

    if (!d) return;
    Date_ToStr(&d->birth, birth, sizeof birth);
    Doctor_ExplainWorkId(d->workId, &dept, &field, &title, &sc, &gc);

    printf("--------------------------------------------------\n");
    printf("  工号      : %s   (科室%d-领域%d-职称%d-姓%02d-名%03d)\n",
           d->workId, dept, field, title, sc, gc);
    printf("  身份证号  : %s\n", d->idCard);
    printf("  姓名      : %s\n", d->name);
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
    d->active = SLOT_INUSE;

    /* 姓名：姓 + 1~2 个名字 */
    if (rand() % 2) {
        snprintf(d->name, NAME_LEN, "%s%s%s",
                 SURNAME_TABLE[rand() % SURNAME_COUNT],
                 RND_GIVEN1[rand() % ARRAY_COUNT(RND_GIVEN1)],
                 RND_GIVEN2[rand() % ARRAY_COUNT(RND_GIVEN2)]);
    } else {
        snprintf(d->name, NAME_LEN, "%s%s",
                 SURNAME_TABLE[rand() % SURNAME_COUNT],
                 RND_GIVEN1[rand() % ARRAY_COUNT(RND_GIVEN1)]);
    }

    d->dept  = (Department)(rand() % DEPT_COUNT);
    d->field = (Field)(rand() % FIELD_COUNT);

    /* 出生日期与性别先定，身份证号由二者派生 */
    d->birth.year  = 1960 + rand() % 41;
    d->birth.month = 1 + rand() % 12;
    d->birth.day   = 1 + rand() % 28;
    d->gender      = (Gender)(rand() % GENDER_COUNT);
    IdCard_Make(d->idCard, &d->birth, d->gender, seq);

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

    /* 职称已确定，最后派生工号 */
    Doctor_BuildWorkId(d);
}
