/**
 * main.c —— 交互层（v2.1）
 * 菜单驱动的控制台界面：入职免输工号、信息检索、简略/详细模式切换、-help 帮助。
 *
 * v2.1 交互约定：
 *   任意输入处输入 q，即放弃当前操作并返回主菜单。
 *
 * 编译示例:
 *   gcc -std=c99 -Wall -Wextra -O2 doctor.c seqlist.c service.c main.c -o doctor.exe
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define DATA_FILE  "data.txt"
#define PAGE_SIZE  15
#define GEN_COUNT  120          /* 首次运行自动生成的测试数据条数 */

/* 全局「放弃当前操作」标志：任意输入处输入 q 即置位，由主循环统一处理 */
static int g_cancelled = 0;

/* ===================== 控制台初始化 ===================== */
static void Console_InitUtf8(void)
{
#ifdef _WIN32
    /* 让 Windows 控制台按 UTF-8 收发，避免中文乱码 */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

/* ===================== 输入工具 ===================== */
static void FlushLine(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

/* 读取一行，去掉行尾换行；空输入会重新提示。返回 0 表示输入流结束 */
static int ReadLine(const char* prompt, char* buf, int size)
{
    for (;;) {
        size_t len;

        printf("%s", prompt);
        fflush(stdout);

        if (!fgets(buf, size, stdin)) {
            buf[0] = '\0';
            return 0;
        }

        len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[--len] = '\0';
            if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';
        } else {
            FlushLine();    /* 输入超长，丢弃剩余字符 */
        }

        /* 任意输入处输入 q：放弃当前操作，返回主菜单 */
        if ((buf[0] == 'q' || buf[0] == 'Q') && buf[1] == '\0') {
            g_cancelled = 1;
            return 0;
        }

        if (buf[0] != '\0') return 1;
        printf("  输入不能为空，请重新输入。\n");
    }
}

/* 读取 [lo, hi] 范围内的整数 */
static int ReadInt(const char* prompt, int lo, int hi, int* out)
{
    char buf[64];

    for (;;) {
        char* end = NULL;
        long  v;

        if (!ReadLine(prompt, buf, sizeof buf)) return 0;

        v = strtol(buf, &end, 10);
        if (end) {
            while (*end == ' ' || *end == '\t') end++;
        }
        if (end && *end == '\0' && v >= lo && v <= hi) {
            *out = (int)v;
            return 1;
        }
        printf("  请输入 %d ~ %d 之间的整数。\n", lo, hi);
    }
}

/* 读取枚举字段：既可输入序号，也可直接输入中文名称 */
static int InputEnum(const char* label, const char* const* table, int count, int* out)
{
    char prompt[256];
    int  n = 0;
    int  i;

    n += snprintf(prompt + n, sizeof(prompt) - (size_t)n, "%s(", label);
    for (i = 0; i < count; i++) {
        n += snprintf(prompt + n, sizeof(prompt) - (size_t)n, "%s%d=%s",
                      i ? " " : "", i + 1, table[i]);
    }
    snprintf(prompt + n, sizeof(prompt) - (size_t)n, "): ");

    for (;;) {
        char buf[32];
        int  v;

        if (!ReadLine(prompt, buf, sizeof buf)) return 0;

        v = atoi(buf);
        if (v >= 1 && v <= count) {
            *out = v - 1;
            return 1;
        }
        if (Enum_Parse(table, count, buf, out)) return 1;

        printf("  无效选项，请输入 1 ~ %d 或直接输入名称。\n", count);
    }
}

static int ReadDate(const char* prompt, Date* out)
{
    char buf[64];

    for (;;) {
        if (!ReadLine(prompt, buf, sizeof buf)) return 0;
        if (Date_FromStr(buf, out)) return 1;
        printf("  日期格式错误，请按 YYYY-MM-DD 输入（年份 1900-2100）。\n");
    }
}

/* 读取 8 位工号 */
static int ReadWorkId(const char* prompt, char* out)
{
    char buf[32];

    for (;;) {
        if (!ReadLine(prompt, buf, sizeof buf)) return 0;
        if (Doctor_WorkIdValid(buf)) {
            memcpy(out, buf, WORKID_LEN);   /* 已验证为 8 位数字，含结束符共 9 字节 */
            return 1;
        }
        printf("  工号必须为 8 位数字（科室1+领域1+职称1+姓2+名3）。\n");
    }
}

/* 读取身份证号并直接写入记录（同时派生性别与出生日期） */
static int ReadIdCardInto(const char* prompt, Doctor* d)
{
    char buf[64];

    for (;;) {
        if (!ReadLine(prompt, buf, sizeof buf)) return 0;
        if (Doctor_SetIdCard(d, buf)) return 1;
        printf("  身份证号校验失败：须为 18 位，前 17 位数字、末位数字或 X，且校验位正确。\n");
    }
}

static int Confirm(const char* prompt)
{
    char buf[16];

    if (!ReadLine(prompt, buf, sizeof buf)) return 0;
    return buf[0] == 'y' || buf[0] == 'Y';
}

/* 读取一次按键行，用于暂停与分页 */
static void ReadKeyLine(char* buf, int size)
{
    if (fgets(buf, size, stdin)) {
        if (strchr(buf, '\n') == NULL) FlushLine();
    } else {
        buf[0] = '\0';
    }
}

static void Pause(void)
{
    char buf[8];

    printf("\n按回车键继续...");
    fflush(stdout);
    ReadKeyLine(buf, sizeof buf);
}

/* ===================== 结果集浏览 ===================== */
/* fullMode = 0 简略模式（不显示身份证号、出生日期、学历、学位、专业）
 * fullMode = 1 详细模式（显示全部字段）
 * 打印结果集 [from, to) 区间的记录，序号沿用结果集中的次序 */
static void PrintSetPage(const SeqList* L, const IndexSet* set, int from, int to, int fullMode)
{
    int i;

    if (fullMode) Doctor_PrintHeaderFull();
    else          Doctor_PrintHeaderBrief();

    for (i = from; i < to; i++) {
        if (fullMode) Doctor_PrintRowFull(i + 1, &L->data[set->idx[i]]);
        else          Doctor_PrintRowBrief(i + 1, &L->data[set->idx[i]]);
    }
}

static int PromptSortKey(SortKey* out)
{
    int c;

    printf("   1. 按工号升序\n");
    printf("   2. 按姓名升序\n");
    printf("   3. 按年龄升序\n");
    printf("   0. 取消\n");
    if (!ReadInt("请选择排序键: ", 0, 3, &c)) return 0;
    if (c == 0) return 0;

    *out = (c == 1) ? SORT_BY_WORKID : (c == 2) ? SORT_BY_NAME : SORT_BY_AGE;
    return 1;
}

/* 结果集浏览：分页显示，可切换简略/详细模式、排序、继续翻页、退出 */
static void BrowseResultSet(const SeqList* L, IndexSet* set, const char* title)
{
    int fullMode = 0;
    int pos      = 0;

    if (set->count == 0) {
        printf("\n===== %s =====\n  没有符合条件的记录。\n", title);
        return;
    }

    for (;;) {
        char buf[16];
        int  end;
        int  quit = 0;

        end = pos + PAGE_SIZE;
        if (end > set->count) end = set->count;

        printf("\n===== %s（%s模式，共 %d 条）=====\n",
               title, fullMode ? "详细" : "简略", set->count);
        PrintSetPage(L, set, pos, end, fullMode);

        printf("===== 已显示 %d/%d 条 =====\n", end, set->count);
        printf("  [s] 简略模式  [d] 详细模式  [o] 排序   [回车] 继续  [q] 退出: ");
        fflush(stdout);

        if (!fgets(buf, sizeof buf, stdin)) return;
        if (strchr(buf, '\n') == NULL) FlushLine();

        switch (buf[0]) {
        case 's': case 'S':
            fullMode = 0;
            break;
        case 'd': case 'D':
            fullMode = 1;
            break;
        case 'o': case 'O': {
            SortKey key;
            if (PromptSortKey(&key)) {
                Service_SortSet(L, set, key);
                pos = 0;
                printf("  已对结果集重新排序（仅调整显示次序，不移动物理栏位）。\n");
            }
            break;
        }
        case 'q': case 'Q':
            quit = 1;
            break;
        default:                        /* 回车：继续显示下一页 */
            if (end >= set->count) {
                printf("  已显示全部记录。\n");
                quit = 1;
            } else {
                pos = end;
            }
            break;
        }

        if (quit) break;
    }
}

/* ===================== 信息检索条件编辑 ===================== */
static void PrintQueryState(const Query* q)
{
    int any = 0;

    printf("\n----- 当前检索条件（条件之间为「与」关系）-----\n");

    if (q->activeFilter == -1)      printf("  人员状态 : 仅在职\n");
    else if (q->activeFilter == 1)  printf("  人员状态 : 仅离职\n");
    else                            printf("  人员状态 : 在职 + 离职（全部栏位）\n");

    if (q->useWorkId) { printf("  工号     : %s\n", q->workId); any = 1; }
    if (q->useWorkIdRange) {
        printf("  工号区间 : %s ~ %s\n", q->workIdLo, q->workIdHi);
        any = 1;
    }
    if (q->useName)   { printf("  姓名     : 含 \"%s\"\n", q->name); any = 1; }
    if (q->useIdCard) { printf("  身份证号 : %s\n", q->idCard); any = 1; }
    if (q->useGender) { printf("  性别     : %s\n", GENDER_NAMES[q->gender]); any = 1; }
    if (q->useAge)    { printf("  年龄区间 : %d ~ %d 岁\n", q->ageLo, q->ageHi); any = 1; }
    if (q->useBirth) {
        char lo[16], hi[16];
        Date_ToStr(&q->birthLo, lo, sizeof lo);
        Date_ToStr(&q->birthHi, hi, sizeof hi);
        printf("  出生日期 : %s ~ %s\n", lo, hi);
        any = 1;
    }
    if (q->useEdu)      { printf("  学历     : %s\n", EDU_NAMES[q->edu]); any = 1; }
    if (q->useDegree)   { printf("  学位     : %s\n", DEG_NAMES[q->degree]); any = 1; }
    if (q->useMajor)    { printf("  专业     : 含 \"%s\"\n", q->major); any = 1; }
    if (q->usePosition) { printf("  职务     : %s\n", POS_NAMES[q->position]); any = 1; }
    if (q->useTitle)    { printf("  职称     : %s\n", TITLE_NAMES[q->title]); any = 1; }
    if (q->useDept)     { printf("  科室     : %s\n", DEPT_NAMES[q->dept]); any = 1; }
    if (q->useField)    { printf("  研究领域 : %s\n", FIELD_NAMES[q->field]); any = 1; }

    if (!any) printf("  （尚未设置具体条件，将按人员状态检索）\n");
}

/* 编辑复合条件：返回 1 表示执行检索，0 表示用户取消 */
static int EditQuery(Query* q)
{
    for (;;) {
        int c;

        PrintQueryState(q);

        printf("\n  1.工号(精确)      2.工号区间       3.姓名(模糊)      4.身份证号(精确)\n");
        printf("  5.性别            6.年龄区间       7.出生日期区间    8.学历\n");
        printf("  9.学位           10.专业(模糊)    11.职务          12.职称\n");
        printf(" 13.科室           14.研究领域      15.人员状态      16.清除全部条件\n");
        printf("  0.执行检索\n");

        if (!ReadInt("请选择要设置的条件: ", 0, 16, &c)) return 0;
        if (c == 0) return 1;

        if (c == 16) {
            int af = q->activeFilter;
            Query_Init(q);
            q->activeFilter = af;
            printf("  已清除全部检索条件。\n");
            continue;
        }

        switch (c) {
        case 1:
            if (ReadWorkId("工号: ", q->workId)) q->useWorkId = 1;
            break;
        case 2: {
            char lo[WORKID_LEN];
            char hi[WORKID_LEN];
            char t[WORKID_LEN];

            if (!ReadWorkId("工号区间下界: ", lo)) return 0;
            if (!ReadWorkId("工号区间上界: ", hi)) return 0;
            if (strcmp(lo, hi) > 0) {           /* 保证下界 <= 上界 */
                memcpy(t, lo, WORKID_LEN);
                memcpy(lo, hi, WORKID_LEN);
                memcpy(hi, t, WORKID_LEN);
            }
            memcpy(q->workIdLo, lo, WORKID_LEN);
            memcpy(q->workIdHi, hi, WORKID_LEN);
            q->useWorkIdRange = 1;
            break;
        }
        case 3:
            if (ReadLine("姓名关键字: ", q->name, NAME_LEN)) q->useName = 1;
            break;
        case 4: {
            Doctor t;
            memset(&t, 0, sizeof t);
            if (ReadIdCardInto("身份证号: ", &t)) {
                snprintf(q->idCard, IDCARD_LEN, "%s", t.idCard);
                q->useIdCard = 1;
            }
            break;
        }
        case 5: {
            int v;
            if (InputEnum("性别", GENDER_NAMES, GENDER_COUNT, &v)) {
                q->gender = (Gender)v;
                q->useGender = 1;
            }
            break;
        }
        case 6: {
            int lo, hi;
            if (!ReadInt("最小年龄: ", 0, 120, &lo)) return 0;
            if (!ReadInt("最大年龄: ", 0, 120, &hi)) return 0;
            if (lo > hi) { int t = lo; lo = hi; hi = t; }
            q->ageLo = lo;
            q->ageHi = hi;
            q->useAge = 1;
            break;
        }
        case 7:
            if (!ReadDate("起始出生日期: ", &q->birthLo)) return 0;
            if (!ReadDate("结束出生日期: ", &q->birthHi)) return 0;
            if (Date_Compare(&q->birthLo, &q->birthHi) > 0) {
                Date t = q->birthLo; q->birthLo = q->birthHi; q->birthHi = t;
            }
            q->useBirth = 1;
            break;
        case 8: {
            int v;
            if (InputEnum("学历", EDU_NAMES, EDU_COUNT, &v)) { q->edu = (Education)v; q->useEdu = 1; }
            break;
        }
        case 9: {
            int v;
            if (InputEnum("学位", DEG_NAMES, DEG_COUNT, &v)) { q->degree = (Degree)v; q->useDegree = 1; }
            break;
        }
        case 10:
            if (ReadLine("专业关键字: ", q->major, STR_LEN)) q->useMajor = 1;
            break;
        case 11: {
            int v;
            if (InputEnum("职务", POS_NAMES, POS_COUNT, &v)) { q->position = (Position)v; q->usePosition = 1; }
            break;
        }
        case 12: {
            int v;
            if (InputEnum("职称", TITLE_NAMES, TITLE_COUNT, &v)) { q->title = (Title)v; q->useTitle = 1; }
            break;
        }
        case 13: {
            int v;
            if (InputEnum("科室", DEPT_NAMES, DEPT_COUNT, &v)) { q->dept = (Department)v; q->useDept = 1; }
            break;
        }
        case 14: {
            int v;
            if (InputEnum("研究领域", FIELD_NAMES, FIELD_COUNT, &v)) { q->field = (Field)v; q->useField = 1; }
            break;
        }
        case 15: {
            int v;
            printf("   1. 仅在职   2. 仅离职   3. 全部\n");
            if (!ReadInt("请选择: ", 1, 3, &v)) return 0;
            q->activeFilter = (v == 1) ? -1 : (v == 2) ? 1 : 0;
            break;
        }
        default:
            break;
        }
    }
}

/* ===================== 各功能处理 ===================== */
static void HandleHire(SeqList* L)
{
    Doctor d;
    char   nameBuf[NAME_LEN];
    char   majorBuf[STR_LEN];
    char   err[128];
    char   birth[16];
    int    v;
    int    slot = 0;
    int    rc;

    printf("\n----- 入职登记（工号自动派生，无需输入）-----\n");
    memset(&d, 0, sizeof d);

    if (!ReadLine("姓名: ", nameBuf, NAME_LEN)) return;
    snprintf(d.name, NAME_LEN, "%s", nameBuf);

    if (!ReadIdCardInto("身份证号(18 位): ", &d)) return;
    Date_ToStr(&d.birth, birth, sizeof birth);
    printf("  由身份证号派生：性别 = %s，出生日期 = %s\n",
           GENDER_NAMES[d.gender], birth);

    if (!InputEnum("学历", EDU_NAMES, EDU_COUNT, &v)) return;
    d.education = (Education)v;

    if (!InputEnum("学位", DEG_NAMES, DEG_COUNT, &v)) return;
    d.degree = (Degree)v;

    if (!ReadLine("专业: ", majorBuf, STR_LEN)) return;
    snprintf(d.major, STR_LEN, "%s", majorBuf);

    if (!InputEnum("职务", POS_NAMES, POS_COUNT, &v)) return;
    d.position = (Position)v;

    if (!InputEnum("职称", TITLE_NAMES, TITLE_COUNT, &v)) return;
    d.title = (Title)v;

    if (!InputEnum("科室", DEPT_NAMES, DEPT_COUNT, &v)) return;
    d.dept = (Department)v;

    if (!InputEnum("研究领域", FIELD_NAMES, FIELD_COUNT, &v)) return;
    d.field = (Field)v;

    /* 工号由 科室+领域+职称+姓+名 自动派生 */
    d.active = SLOT_INUSE;
    Doctor_BuildWorkId(&d);

    if (!Doctor_Validate(&d, err, sizeof err)) {
        printf("  数据校验失败: %s\n", err);
        return;
    }

    Doctor_PrintDetail(&d);
    printf("  提示: 系统将按「重用工号 -> 复用空位 -> 开辟新栏位」的顺序落位。\n");
    if (!Confirm("确认办理入职？(y/N): ")) {
        printf("  已取消。\n");
        return;
    }

    rc = Service_Hire(L, &d, &slot);
    switch (rc) {
    case HIRE_OK_REUSE_WORKID:
        printf("  入职成功：重用离职老员工工号 %s，落回原栏位 #%d。\n", d.workId, slot);
        break;
    case HIRE_OK_REUSE_SLOT:
        printf("  入职成功：复用最靠前的空位 #%d，工号为 %s。\n", slot, d.workId);
        break;
    case HIRE_OK_NEW_SLOT:
        printf("  入职成功：人员真实扩充，新开辟栏位 #%d，工号为 %s。\n", slot, d.workId);
        break;
    case HIRE_FAIL_DUP_WORKID:
        printf("  入职失败：派生出的工号 %s 已被在岗人员占用。\n", d.workId);
        break;
    case HIRE_FAIL_DUP_IDCARD:
        printf("  入职失败：身份证号 %s 已存在于在岗人员中。\n", d.idCard);
        break;
    default:
        printf("  入职失败：内存不足。\n");
        break;
    }
}

static void HandleResign(SeqList* L)
{
    char workId[WORKID_LEN];
    int  pos;

    printf("\n----- 离职登记（逻辑删除）-----\n");
    if (!ReadWorkId("请输入离职人员工号: ", workId)) return;

    pos = Service_FindIndexByWorkId(L, workId);
    if (pos < 0) {
        printf("  未找到在岗工号 %s 的记录。\n", workId);
        return;
    }

    Doctor_PrintDetail(&L->data[pos]);
    if (!Confirm("确认登记离职？(y/N): ")) {
        printf("  已取消。\n");
        return;
    }

    Service_Resign(L, workId);
    printf("  已登记离职：栏位 #%d 的不外显标识符切换为「空位」。\n", pos + 1);
    printf("  后续人员工号不前移，该工号可被新入职员工重用。\n");
}

static void HandleCompositeQuery(const SeqList* L)
{
    Query    q;
    IndexSet set;

    printf("\n----- 信息检索 -----\n");
    Query_Init(&q);
    if (!EditQuery(&q)) {
        printf("  已取消。\n");
        return;
    }

    Service_Query(L, &q, &set);
    BrowseResultSet(L, &set, "信息检索结果");
}

static void HandleUpdate(SeqList* L)
{
    char   workId[WORKID_LEN];
    char   nameBuf[NAME_LEN];
    char   majorBuf[STR_LEN];
    char   err[128];
    char   birth[16];
    Doctor* d;
    int    pos;
    int    choice;
    int    v;

    printf("\n----- 修改记录 -----\n");
    if (!ReadWorkId("请输入要修改的工号: ", workId)) return;

    pos = Service_FindIndexByWorkId(L, workId);
    if (pos < 0) {
        printf("  未找到在岗工号 %s 的记录。\n", workId);
        return;
    }
    d = &L->data[pos];

    Doctor_PrintDetail(d);

    for (;;) {
        printf("\n可修改字段:\n");
        printf("   1.姓名          2.身份证号(同步性别/出生日期)\n");
        printf("   3.学历          4.学位            5.专业\n");
        printf("   6.职务          7.职称(重算工号)  8.科室(重算工号)\n");
        printf("   9.研究领域      0.完成\n");

        if (!ReadInt("请选择: ", 0, 9, &choice)) return;
        if (choice == 0) break;

        switch (choice) {
        case 1:
            if (!ReadLine("新姓名: ", nameBuf, NAME_LEN)) return;
            snprintf(d->name, NAME_LEN, "%s", nameBuf);
            break;
        case 2:
            if (!ReadIdCardInto("新身份证号: ", d)) return;
            Date_ToStr(&d->birth, birth, sizeof birth);
            printf("  已同步：性别 = %s，出生日期 = %s\n", GENDER_NAMES[d->gender], birth);
            break;
        case 3:
            if (!InputEnum("学历", EDU_NAMES, EDU_COUNT, &v)) return;
            d->education = (Education)v;
            break;
        case 4:
            if (!InputEnum("学位", DEG_NAMES, DEG_COUNT, &v)) return;
            d->degree = (Degree)v;
            break;
        case 5:
            if (!ReadLine("新专业: ", majorBuf, STR_LEN)) return;
            snprintf(d->major, STR_LEN, "%s", majorBuf);
            break;
        case 6:
            if (!InputEnum("职务", POS_NAMES, POS_COUNT, &v)) return;
            d->position = (Position)v;
            break;
        case 7: {
            Title old;
            if (!InputEnum("职称", TITLE_NAMES, TITLE_COUNT, &v)) return;
            old = d->title;
            d->title = (Title)v;
            if (!Service_RebuildWorkId(L, pos)) {
                d->title = old;
                printf("  修改失败：重算后的工号与其它在岗人员冲突，已回滚。\n");
            } else {
                printf("  已更新，工号重算为 %s。\n", d->workId);
            }
            break;
        }
        case 8: {
            Department old;
            if (!InputEnum("科室", DEPT_NAMES, DEPT_COUNT, &v)) return;
            old = d->dept;
            d->dept = (Department)v;
            if (!Service_RebuildWorkId(L, pos)) {
                d->dept = old;
                printf("  修改失败：重算后的工号与其它在岗人员冲突，已回滚。\n");
            } else {
                printf("  已更新，工号重算为 %s。\n", d->workId);
            }
            break;
        }
        case 9:
            if (!InputEnum("研究领域", FIELD_NAMES, FIELD_COUNT, &v)) return;
            d->field = (Field)v;
            break;
        default:
            break;
        }

        if (!Doctor_Validate(d, err, sizeof err)) printf("  警告: %s\n", err);
        else                                      printf("  已更新。\n");
    }

    printf("  修改完成。\n");
}

static void HandleRangeUpdate(SeqList* L)
{
    Query    q;
    IndexSet set;
    int      choice;
    int      v = -1;
    int      changed = 0;
    int      conflict = 0;

    printf("\n----- 区间修改 -----\n");
    printf("  先构造信息检索条件筛出目标记录，再执行批量修改。\n");

    Query_Init(&q);
    if (!EditQuery(&q)) {
        printf("  已取消。\n");
        return;
    }

    Service_Query(L, &q, &set);
    if (set.count == 0) {
        printf("  没有符合条件的记录，已取消。\n");
        return;
    }

    BrowseResultSet(L, &set, "待修改记录");

    printf("\n批量操作:\n");
    printf("   1. 职称晋升一级（重算工号）\n");
    printf("   2. 学历提升一级（学位同步）\n");
    printf("   3. 统一调整科室（重算工号）\n");
    printf("   0. 取消\n");

    if (!ReadInt("请选择: ", 0, 3, &choice)) return;
    if (choice == 0) {
        printf("  已取消。\n");
        return;
    }
    if (choice == 3 && !InputEnum("目标科室", DEPT_NAMES, DEPT_COUNT, &v)) return;

    if (!Confirm("确认对以上记录执行修改？(y/N): ")) {
        printf("  已取消。\n");
        return;
    }

    switch (choice) {
    case 1: changed = Service_PromoteTitle(L, &set, &conflict); break;
    case 2: changed = Service_PromoteEducation(L, &set, &conflict); break;
    case 3: changed = Service_ChangeDept(L, &set, (Department)v, &conflict); break;
    default: break;
    }

    printf("  修改完成，共更新 %d 条记录。\n", changed);
    if (conflict > 0) printf("  有 %d 条因工号重算冲突被回滚（该工号已被在岗人员占用）。\n", conflict);
    BrowseResultSet(L, &set, "修改后记录");
}

static void HandleStats(const SeqList* L)
{
    Query    q;
    IndexSet set;
    Stats    st;

    printf("\n----- 区间统计 -----\n");
    Query_Init(&q);
    if (!EditQuery(&q)) {
        printf("  已取消。\n");
        return;
    }

    Service_Query(L, &q, &set);
    Service_StatsSet(L, &set, &st);
    Service_PrintStats(&st);
}

static void HandleFile(SeqList* L)
{
    int choice;

    for (;;) {
        printf("\n----- 文件与数据 -----\n");
        printf("  栏位总数: %d    在岗: %d    空位: %d\n",
               L->length, Service_ActiveCount(L), Service_VacantCount(L));
        printf("   1. 保存到 %s\n", DATA_FILE);
        printf("   2. 从 %s 加载（覆盖当前数据）\n", DATA_FILE);
        printf("   3. 随机生成测试数据\n");
        printf("   4. 清理全部空位（物理删除，后续栏位前移）\n");
        printf("   5. 清空全部数据\n");
        printf("   0. 返回\n");

        if (!ReadInt("请选择: ", 0, 5, &choice)) return;

        switch (choice) {
        case 0:
            return;
        case 1:
            if (Service_Save(L, DATA_FILE)) printf("  已保存 %d 个栏位到 %s。\n", L->length, DATA_FILE);
            else                            printf("  保存失败，请检查目录写入权限。\n");
            break;
        case 2: {
            int skipped = 0;
            int n = Service_Load(L, DATA_FILE, &skipped);
            if (n < 0) printf("  无法打开 %s。\n", DATA_FILE);
            else       printf("  已加载 %d 条记录，跳过 %d 条无效数据。\n", n, skipped);
            break;
        }
        case 3: {
            int n;
            if (!ReadInt("生成条数(1-1000): ", 1, 1000, &n)) return;
            n = Service_Generate(L, n);
            printf("  已生成 %d 条记录，当前栏位总数 %d。\n", n, L->length);
            break;
        }
        case 4: {
            int n = Service_PurgeVacant(L);
            printf("  已清理 %d 个空位，当前栏位总数 %d。\n", n, L->length);
            break;
        }
        case 5:
            if (Confirm("确认清空全部数据？(y/N): ")) {
                List_Clear(L);
                printf("  已清空全部数据。\n");
            } else {
                printf("  已取消。\n");
            }
            break;
        default:
            break;
        }

        Pause();
    }
}

/* ===================== 帮助 ===================== */
static void PrintHelp(void)
{
    printf("\n");
    printf("==================================================================\n");
    printf("                博士(医师)信息管理系统  v2.1  -help\n");
    printf("==================================================================\n");
    printf("用法:\n");
    printf("  doctor               启动交互式菜单\n");
    printf("  doctor -help         显示本帮助并退出\n");
    printf("  doctor -h / --help   同上\n");
    printf("\n");
    printf("系统简介:\n");
    printf("  采用顺序表(SeqList)存储 100+ 条博士(医师)信息，支持入职、离职、\n");
    printf("  信息检索、区间修改、区间统计等操作。\n");
    printf("  程序启动时自动读取 data.txt；若文件不存在或内容无效则随机生成 %d 条数据。\n", GEN_COUNT);
    printf("\n");
    printf("操作约定:\n");
    printf("  任意输入处输入 q，即放弃当前操作并返回主菜单；\n");
    printf("  菜单选择处输入 0，表示取消/返回上一层。\n");
    printf("\n");
    printf("v2.1 关键设计:\n");
    printf("  1. 工号与身份证号分离\n");
    printf("       工号  : 8 位结构化编号 = 科室(1)+领域(1)+职称(1)+姓(2)+名(3)\n");
    printf("               由属性自动派生，录入时无需输入，也不允许手工指定\n");
    printf("       身份证: 18 位真实身份标识，含校验位，并派生性别与出生日期\n");
    printf("  2. 不外显标识符 active\n");
    printf("       离职只把该栏位切换为「空位」，后续工号不前移；\n");
    printf("       新员工落位顺序 = 重用离职工号 -> 复用最靠前空位 -> 真实扩充新栏位\n");
    printf("  3. 显示模式切换\n");
    printf("       简略模式不显示身份证号、出生日期、学历、学位、专业；\n");
    printf("       详细模式显示全部字段。两种模式在结果浏览界面按 s / d 切换\n");
    printf("\n");
    printf("菜单功能:\n");
    printf("   1  入职登记     免输工号，自动派生 8 位工号并按序落位\n");
    printf("   2  离职登记     逻辑删除：切换不外显标识符，后续工号不前移\n");
    printf("   3  信息检索     16 项条件自由组合（与关系），结果可切换简略/详细、可排序\n");
    printf("   4  信息修改     按工号定位后修改字段，职称/科室变更会自动重算工号\n");
    printf("   5  区间修改     对检索结果批量晋升职称、提升学历、调整科室\n");
    printf("   6  区间统计     统计检索结果的人数、年龄分布与各类别构成\n");
    printf("   7  文件与数据   保存、加载、生成测试数据、清理空位、清空\n");
    printf("   h  帮助         显示本帮助（等同于 -help）\n");
    printf("   0  退出         保存数据并退出程序\n");
    printf("\n");
    printf("可检索项（信息检索，共 16 项）:\n");
    printf("  工号精确、工号区间、姓名、身份证号、性别、年龄区间、出生日期区间、\n");
    printf("  学历、学位、专业、职务、职称、科室、研究领域、人员状态（在职/离职）\n");
    printf("  提示: 不设置任何条件直接执行检索，即可列出全部在岗人员。\n");
    printf("\n");
    printf("结果浏览按键:\n");
    printf("  [s] 简略模式   [d] 详细模式   [o] 排序   [回车] 继续翻页   [q] 退出\n");
    printf("\n");
    printf("数据文件:\n");
    printf("  %s（与程序同目录，字段以 '|' 分隔，枚举字段以序号存储）\n", DATA_FILE);
    printf("  字段顺序: workId|idCard|active|name|gender|birth|education|degree|\n");
    printf("             major|position|title|dept|field\n");
    printf("==================================================================\n");
}

/* ===================== 主菜单 ===================== */
static void PrintMainMenu(const SeqList* L)
{
    printf("\n");
    printf("==================================================\n");
    printf("            博士(医师)信息管理系统  v2.1\n");
    printf("==================================================\n");
    printf("  栏位总数: %-5d 在岗: %-5d 空位: %-5d\n",
           L->length, Service_ActiveCount(L), Service_VacantCount(L));
    printf("  数据文件: %s\n", DATA_FILE);
    printf("--------------------------------------------------\n");
    printf("   1. 入职登记            2. 离职登记\n");
    printf("   3. 信息检索            4. 信息修改\n");
    printf("   5. 区间修改            6. 区间统计\n");
    printf("   7. 文件与数据\n");
    printf("   h. 帮助(-help)         0. 退出\n");
    printf("--------------------------------------------------\n");
    printf("  提示: 任意输入处输入 q 可放弃当前操作\n");
}

/* ===================== 入口 ===================== */
int main(int argc, char* argv[])
{
    SeqList* L;
    int      i;
    int      loaded;
    int      skipped = 0;

    Console_InitUtf8();

    /* 命令行 -help */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-help") == 0 ||
            strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help") == 0) {
            PrintHelp();
            return 0;
        }
        printf("未知参数: %s\n", argv[i]);
        PrintHelp();
        return 1;
    }

    srand((unsigned)time(NULL));

    L = List_Create(LIST_INIT_CAPACITY);
    if (!L) {
        printf("内存分配失败，程序退出。\n");
        return 1;
    }

    loaded = Service_Load(L, DATA_FILE, &skipped);
    if (loaded < 0) {
        printf("未找到数据文件 %s，自动生成 %d 条测试数据。\n", DATA_FILE, GEN_COUNT);
        Service_Generate(L, GEN_COUNT);
    } else if (loaded == 0 && skipped > 0) {
        printf("数据文件 %s 的 %d 条记录格式不符（可能为旧版本），已重新生成 %d 条测试数据。\n",
               DATA_FILE, skipped, GEN_COUNT);
        Service_Generate(L, GEN_COUNT);
    } else {
        printf("已从 %s 加载 %d 条记录（在岗 %d，空位 %d）。\n",
               DATA_FILE, loaded, Service_ActiveCount(L), Service_VacantCount(L));
    }

    for (;;) {
        char buf[16];

        PrintMainMenu(L);

        g_cancelled = 0;
        if (!ReadLine("请选择操作: ", buf, sizeof buf)) {
            if (g_cancelled) { g_cancelled = 0; continue; }   /* 主菜单处输入 q 只忽略 */
            break;                                            /* 输入流结束则退出 */
        }

        if (strcmp(buf, "0") == 0) break;
        if (strcmp(buf, "h") == 0 || strcmp(buf, "H") == 0 || strcmp(buf, "-help") == 0) {
            PrintHelp();
            Pause();
            continue;
        }

        switch (atoi(buf)) {
        case 1:  HandleHire(L);           break;
        case 2:  HandleResign(L);         break;
        case 3:  HandleCompositeQuery(L); break;
        case 4:  HandleUpdate(L);         break;
        case 5:  HandleRangeUpdate(L);    break;
        case 6:  HandleStats(L);          break;
        case 7:  HandleFile(L);           break;
        default: printf("  无效选项，请输入 0-7 或 h。\n");
        }

        /* 操作过程中输入 q：放弃当前操作，直接返回主菜单 */
        if (g_cancelled) {
            g_cancelled = 0;
            printf("\n  已放弃当前操作，返回主菜单。\n");
            continue;
        }

        Pause();
    }

    if (Service_Save(L, DATA_FILE)) printf("\n数据已保存到 %s。\n", DATA_FILE);
    else                            printf("\n警告: 数据保存失败。\n");

    List_Destroy(L);
    printf("程序已退出。\n");
    return 0;
}
