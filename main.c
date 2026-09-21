/**
 * main.c —— 交互层
 * 菜单驱动的控制台界面：输入校验、分页输出、-help 帮助。
 *
 * 编译示例:
 *   gcc -std=c99 -Wall -O2 doctor.c seqlist.c service.c main.c -o doctor
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define DATA_FILE  "data.txt"
#define PAGE_SIZE  15
#define GEN_COUNT  120          /* 首次运行自动生成的测试数据条数 */

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

static int ReadId(const char* prompt, char* out)
{
    char  buf[64];
    Doctor tmp;

    for (;;) {
        if (!ReadLine(prompt, buf, sizeof buf)) return 0;
        memset(&tmp, 0, sizeof tmp);
        if (Doctor_SetId(&tmp, buf)) {
            memcpy(out, tmp.id, ID_LEN);
            return 1;
        }
        printf("  工号必须是 18 位数字。\n");
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

/* ===================== 分页输出 ===================== */
static void PrintSetPaged(const SeqList* L, const IndexSet* set, const char* title)
{
    int i;

    printf("\n===== %s（共 %d 条）=====\n", title, set->count);
    if (set->count == 0) {
        printf("  没有符合条件的记录。\n");
        return;
    }

    Doctor_PrintHeader();
    for (i = 0; i < set->count; i++) {
        Doctor_PrintRow(i + 1, &L->data[set->idx[i]]);

        if ((i + 1) % PAGE_SIZE == 0 && i + 1 < set->count) {
            char buf[8];
            printf("-- 已显示 %d/%d 条，按回车继续，输入 q 结束浏览 --\n",
                   i + 1, set->count);
            ReadKeyLine(buf, sizeof buf);
            if (buf[0] == 'q' || buf[0] == 'Q') {
                printf("  已结束浏览。\n");
                return;
            }
        }
    }
    printf("===== 显示完毕 =====\n");
}

/* ===================== 帮助 ===================== */
static void PrintHelp(void)
{
    printf("\n");
    printf("==================================================================\n");
    printf("                博士(医师)信息管理系统  -help\n");
    printf("==================================================================\n");
    printf("用法:\n");
    printf("  doctor               启动交互式菜单\n");
    printf("  doctor -help         显示本帮助并退出\n");
    printf("  doctor -h / --help   同上\n");
    printf("\n");
    printf("系统简介:\n");
    printf("  采用顺序表(SeqList)存储 100+ 条博士(医师)信息，支持增、删、\n");
    printf("  查、改，以及区间查找、区间修改、区间统计等复杂操作。\n");
    printf("  程序启动时自动读取 data.txt；若文件不存在则随机生成 %d 条数据。\n", GEN_COUNT);
    printf("\n");
    printf("菜单功能:\n");
    printf("   1  插入记录     按工号有序插入一条新记录，工号重复会被拒绝\n");
    printf("   2  删除记录     按工号定位并删除记录\n");
    printf("   3  查找记录     按工号精确查找 / 按姓名模糊查找 / 显示全部\n");
    printf("   4  修改记录     按工号定位后修改任意字段\n");
    printf("   5  显示全部     分页显示全部记录，每页 %d 条\n", PAGE_SIZE);
    printf("   6  区间查找     按工号区间 / 出生日期区间 / 年龄区间 / 科室筛选\n");
    printf("   7  区间修改     对筛选结果批量晋升职称、提升学历、调整科室\n");
    printf("   8  区间统计     统计筛选结果的人数、年龄分布与各类别构成\n");
    printf("   9  排序         按工号 / 姓名 / 年龄升序排序\n");
    printf("  10  文件         保存、加载 data.txt，生成测试数据，清空数据\n");
    printf("   h  帮助         显示本帮助（等同于 -help）\n");
    printf("   0  退出         保存数据并退出程序\n");
    printf("\n");
    printf("数据字段:\n");
    printf("  姓名、工号(18 位)、性别、出生日期、学历、学位、专业、\n");
    printf("  职务、职称、科室、研究领域\n");
    printf("\n");
    printf("数据文件:\n");
    printf("  %s（与程序同目录，字段以 '|' 分隔，枚举字段以序号存储）\n", DATA_FILE);
    printf("==================================================================\n");
}

/* ===================== 主菜单 ===================== */
static void PrintMainMenu(const SeqList* L)
{
    printf("\n");
    printf("==================================================\n");
    printf("            博士(医师)信息管理系统  v1.0\n");
    printf("==================================================\n");
    printf("  当前记录数: %-6d     数据文件: %s\n", L->length, DATA_FILE);
    printf("--------------------------------------------------\n");
    printf("   1. 插入记录            2. 删除记录\n");
    printf("   3. 查找记录            4. 修改记录\n");
    printf("   5. 显示全部(分页)      6. 区间查找\n");
    printf("   7. 区间修改            8. 区间统计\n");
    printf("   9. 排序               10. 文件与测试数据\n");
    printf("   h. 帮助(-help)         0. 退出\n");
    printf("--------------------------------------------------\n");
}

/* ===================== 筛选条件构造 ===================== */
/* 返回 1 表示已生成结果集，0 表示用户取消 */
static int BuildFilterSet(const SeqList* L, IndexSet* out)
{
    int choice;

    printf("\n----- 选择筛选条件 -----\n");
    printf("   1. 全部记录\n");
    printf("   2. 按工号区间\n");
    printf("   3. 按出生日期区间\n");
    printf("   4. 按年龄区间\n");
    printf("   5. 按科室\n");
    printf("   0. 取消\n");

    if (!ReadInt("请选择: ", 0, 5, &choice)) return 0;

    switch (choice) {
    case 1:
        Service_SelectAll(L, out);
        return 1;
    case 2: {
        char lo[ID_LEN];
        char hi[ID_LEN];
        if (!ReadId("起始工号: ", lo)) return 0;
        if (!ReadId("结束工号: ", hi)) return 0;
        Service_SelectByIdRange(L, lo, hi, out);
        return 1;
    }
    case 3: {
        Date lo;
        Date hi;
        if (!ReadDate("起始出生日期: ", &lo)) return 0;
        if (!ReadDate("结束出生日期: ", &hi)) return 0;
        Service_SelectByBirthRange(L, &lo, &hi, out);
        return 1;
    }
    case 4: {
        int lo;
        int hi;
        if (!ReadInt("最小年龄: ", 0, 120, &lo)) return 0;
        if (!ReadInt("最大年龄: ", 0, 120, &hi)) return 0;
        if (lo > hi) { int t = lo; lo = hi; hi = t; }
        Service_SelectByAgeRange(L, lo, hi, out);
        return 1;
    }
    case 5: {
        int v;
        if (!InputEnum("科室", DEPT_NAMES, DEPT_COUNT, &v)) return 0;
        Service_SelectByDept(L, (Department)v, out);
        return 1;
    }
    default:
        return 0;
    }
}

/* ===================== 各功能处理 ===================== */
static void HandleInsert(SeqList* L)
{
    Doctor d;
    char   nameBuf[NAME_LEN];
    char   majorBuf[STR_LEN];
    char   err[128];
    int    v;

    printf("\n----- 插入记录 -----\n");
    memset(&d, 0, sizeof d);

    if (!ReadLine("姓名: ", nameBuf, NAME_LEN)) return;
    snprintf(d.name, NAME_LEN, "%s", nameBuf);

    if (!ReadId("工号(18 位数字): ", d.id)) return;
    if (Service_FindById(L, d.id) != NULL) {
        printf("  工号 %s 已存在，插入失败。\n", d.id);
        return;
    }

    if (!InputEnum("性别", GENDER_NAMES, GENDER_COUNT, &v)) return;
    d.gender = (Gender)v;

    if (!ReadDate("出生日期(YYYY-MM-DD): ", &d.birth)) return;

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

    if (!Doctor_Validate(&d, err, sizeof err)) {
        printf("  数据校验失败: %s\n", err);
        return;
    }

    Doctor_PrintDetail(&d);
    if (!Confirm("确认插入该记录？(y/N): ")) {
        printf("  已取消。\n");
        return;
    }

    switch (Service_Add(L, &d)) {
    case 1:  printf("  插入成功，当前共 %d 条记录。\n", L->length); break;
    case -1: printf("  插入失败：工号重复。\n"); break;
    default: printf("  插入失败：内存不足。\n"); break;
    }
}

static void HandleDelete(SeqList* L)
{
    char    id[ID_LEN];
    Doctor* d;

    printf("\n----- 删除记录 -----\n");
    if (!ReadId("请输入要删除的工号: ", id)) return;

    d = Service_FindById(L, id);
    if (!d) {
        printf("  未找到工号 %s 的记录。\n", id);
        return;
    }

    Doctor_PrintDetail(d);
    if (!Confirm("确认删除该记录？(y/N): ")) {
        printf("  已取消。\n");
        return;
    }

    if (Service_RemoveById(L, id)) printf("  删除成功，当前共 %d 条记录。\n", L->length);
    else                           printf("  删除失败。\n");
}

static void HandleFind(SeqList* L)
{
    IndexSet set;
    int      choice;

    printf("\n----- 查找记录 -----\n");
    printf("   1. 按工号精确查找\n");
    printf("   2. 按姓名模糊查找\n");
    printf("   3. 显示全部\n");
    printf("   0. 返回\n");

    if (!ReadInt("请选择: ", 0, 3, &choice)) return;

    switch (choice) {
    case 1: {
        char          id[ID_LEN];
        const Doctor* d;
        if (!ReadId("请输入工号: ", id)) return;
        d = Service_FindById(L, id);
        if (!d) {
            printf("  未找到工号 %s 的记录。\n", id);
            return;
        }
        Doctor_PrintDetail(d);
        return;
    }
    case 2: {
        char name[NAME_LEN];
        if (!ReadLine("请输入姓名关键字: ", name, NAME_LEN)) return;
        Service_FindByName(L, name, &set);
        PrintSetPaged(L, &set, "按姓名查找结果");
        return;
    }
    case 3:
        Service_SelectAll(L, &set);
        PrintSetPaged(L, &set, "全部记录");
        return;
    default:
        return;
    }
}

static void HandleUpdate(SeqList* L)
{
    char    id[ID_LEN];
    char    nameBuf[NAME_LEN];
    char    majorBuf[STR_LEN];
    Doctor* d;
    int     choice;
    int     v;

    printf("\n----- 修改记录 -----\n");
    if (!ReadId("请输入要修改的工号: ", id)) return;

    d = Service_FindById(L, id);
    if (!d) {
        printf("  未找到工号 %s 的记录。\n", id);
        return;
    }

    Doctor_PrintDetail(d);

    for (;;) {
        char err[128];

        printf("\n可修改字段:\n");
        printf("   1.姓名        2.性别        3.出生日期    4.学历\n");
        printf("   5.学位        6.专业        7.职务        8.职称\n");
        printf("   9.科室       10.研究领域   11.工号        0.完成\n");

        if (!ReadInt("请选择: ", 0, 11, &choice)) return;
        if (choice == 0) break;

        switch (choice) {
        case 1:
            if (!ReadLine("新姓名: ", nameBuf, NAME_LEN)) return;
            snprintf(d->name, NAME_LEN, "%s", nameBuf);
            break;
        case 2:
            if (!InputEnum("性别", GENDER_NAMES, GENDER_COUNT, &v)) return;
            d->gender = (Gender)v;
            break;
        case 3:
            if (!ReadDate("新出生日期(YYYY-MM-DD): ", &d->birth)) return;
            break;
        case 4:
            if (!InputEnum("学历", EDU_NAMES, EDU_COUNT, &v)) return;
            d->education = (Education)v;
            break;
        case 5:
            if (!InputEnum("学位", DEG_NAMES, DEG_COUNT, &v)) return;
            d->degree = (Degree)v;
            break;
        case 6:
            if (!ReadLine("新专业: ", majorBuf, STR_LEN)) return;
            snprintf(d->major, STR_LEN, "%s", majorBuf);
            break;
        case 7:
            if (!InputEnum("职务", POS_NAMES, POS_COUNT, &v)) return;
            d->position = (Position)v;
            break;
        case 8:
            if (!InputEnum("职称", TITLE_NAMES, TITLE_COUNT, &v)) return;
            d->title = (Title)v;
            break;
        case 9:
            if (!InputEnum("科室", DEPT_NAMES, DEPT_COUNT, &v)) return;
            d->dept = (Department)v;
            break;
        case 10:
            if (!InputEnum("研究领域", FIELD_NAMES, FIELD_COUNT, &v)) return;
            d->field = (Field)v;
            break;
        case 11: {
            char   newId[ID_LEN];
            Doctor copy;

            if (!ReadId("新工号: ", newId)) return;
            if (strcmp(newId, d->id) == 0) {
                printf("  工号未发生变化。\n");
                break;
            }
            if (Service_FindById(L, newId) != NULL) {
                printf("  工号 %s 已存在。\n", newId);
                break;
            }

            copy = *d;
            memcpy(copy.id, newId, ID_LEN);
            Service_RemoveById(L, id);   /* 删除旧记录 */
            Service_Add(L, &copy);       /* 以新工号重新插入 */
            printf("  工号已修改，记录已重新排序。\n");
            return;                      /* d 已失效，结束修改流程 */
        }
        default:
            break;
        }

        if (!Doctor_Validate(d, err, sizeof err)) printf("  警告: %s\n", err);
        else                                      printf("  已更新。\n");
    }

    printf("  修改完成。\n");
}

static void HandleRangeFind(const SeqList* L)
{
    IndexSet set;

    printf("\n----- 区间查找 -----\n");
    if (!BuildFilterSet(L, &set)) {
        printf("  已取消。\n");
        return;
    }
    PrintSetPaged(L, &set, "区间查找结果");
}

static void HandleRangeUpdate(SeqList* L)
{
    IndexSet set;
    int      choice;
    int      v = -1;
    int      changed = 0;

    printf("\n----- 区间修改 -----\n");
    printf("  先筛选目标记录，再执行批量修改。\n");

    if (!BuildFilterSet(L, &set)) {
        printf("  已取消。\n");
        return;
    }
    if (set.count == 0) {
        printf("  没有符合条件的记录，已取消。\n");
        return;
    }

    PrintSetPaged(L, &set, "待修改记录");

    printf("\n批量操作:\n");
    printf("   1. 职称晋升一级\n");
    printf("   2. 学历提升一级（学位同步）\n");
    printf("   3. 统一调整科室\n");
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
    case 1: changed = Service_PromoteTitle(L, &set); break;
    case 2: changed = Service_PromoteEducation(L, &set); break;
    case 3: changed = Service_ChangeDept(L, &set, (Department)v); break;
    default: break;
    }

    printf("  修改完成，共更新 %d 条记录。\n", changed);
    PrintSetPaged(L, &set, "修改后记录");
}

static void HandleStats(const SeqList* L)
{
    IndexSet set;
    Stats    st;

    printf("\n----- 区间统计 -----\n");
    if (!BuildFilterSet(L, &set)) {
        printf("  已取消。\n");
        return;
    }

    Service_StatsSet(L, &set, &st);
    Service_PrintStats(&st);
}

static void HandleSort(SeqList* L)
{
    IndexSet set;
    int      choice;

    printf("\n----- 排序 -----\n");
    printf("   1. 按工号升序\n");
    printf("   2. 按姓名升序\n");
    printf("   3. 按年龄升序\n");
    printf("   0. 返回\n");

    if (!ReadInt("请选择: ", 0, 3, &choice)) return;

    switch (choice) {
    case 1: Service_SortById(L);   printf("  已按工号升序排序。\n"); break;
    case 2: Service_SortByName(L); printf("  已按姓名升序排序。\n"); break;
    case 3: Service_SortByAge(L);  printf("  已按年龄升序排序。\n"); break;
    default: return;
    }

    Service_SelectAll(L, &set);
    PrintSetPaged(L, &set, "排序结果");
}

static void HandleFile(SeqList* L)
{
    int choice;

    for (;;) {
        printf("\n----- 文件与测试数据 -----\n");
        printf("  当前记录数: %d\n", L->length);
        printf("   1. 保存到 %s\n", DATA_FILE);
        printf("   2. 从 %s 加载（覆盖当前数据）\n", DATA_FILE);
        printf("   3. 随机生成测试数据\n");
        printf("   4. 清空全部数据\n");
        printf("   0. 返回\n");

        if (!ReadInt("请选择: ", 0, 4, &choice)) return;

        switch (choice) {
        case 0:
            return;
        case 1:
            if (Service_Save(L, DATA_FILE)) printf("  已保存 %d 条记录到 %s。\n", L->length, DATA_FILE);
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
            printf("  已生成 %d 条记录，当前共 %d 条。\n", n, L->length);
            break;
        }
        case 4:
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

/* ===================== 入口 ===================== */
int main(int argc, char* argv[])
{
    SeqList* L;
    int      i;
    int      loaded;

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

    loaded = Service_Load(L, DATA_FILE, NULL);
    if (loaded < 0) {
        printf("未找到数据文件 %s，自动生成 %d 条测试数据。\n", DATA_FILE, GEN_COUNT);
        Service_Generate(L, GEN_COUNT);
    } else {
        printf("已从 %s 加载 %d 条记录。\n", DATA_FILE, loaded);
    }

    for (;;) {
        char buf[16];

        PrintMainMenu(L);
        if (!ReadLine("请选择操作: ", buf, sizeof buf)) break;   /* 输入流结束则退出 */

        if (strcmp(buf, "0") == 0) break;
        if (strcmp(buf, "h") == 0 || strcmp(buf, "H") == 0 || strcmp(buf, "-help") == 0) {
            PrintHelp();
            Pause();
            continue;
        }

        switch (atoi(buf)) {
        case 1:  HandleInsert(L);       break;
        case 2:  HandleDelete(L);       break;
        case 3:  HandleFind(L);         break;
        case 4:  HandleUpdate(L);       break;
        case 5: {
            IndexSet set;
            Service_SelectAll(L, &set);
            PrintSetPaged(L, &set, "全部记录");
            break;
        }
        case 6:  HandleRangeFind(L);    break;
        case 7:  HandleRangeUpdate(L);  break;
        case 8:  HandleStats(L);        break;
        case 9:  HandleSort(L);         break;
        case 10: HandleFile(L);         break;
        default: printf("  无效选项，请输入 0-10 或 h。\n");
        }

        Pause();
    }

    if (Service_Save(L, DATA_FILE)) printf("\n数据已保存到 %s。\n", DATA_FILE);
    else                            printf("\n警告: 数据保存失败。\n");

    List_Destroy(L);
    printf("程序已退出。\n");
    return 0;
}
