/**
 * service.c —— 业务层实现
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================== 内部工具 ===================== */
static void Set_Push(IndexSet* out, int idx)
{
    if (out->count < MAX_RESULT) {
        out->idx[out->count++] = idx;
    }
}

/* ===================== 基本增删查改 ===================== */
int Service_Add(SeqList* L, const Doctor* d)
{
    int i;
    int pos;

    if (!L || !d) return -1;

    /* 第一遍：查重 */
    for (i = 0; i < L->length; i++) {
        if (strcmp(L->data[i].id, d->id) == 0) return -1;
    }

    /* 第二遍：定位插入点，使列表在按工号有序时保持有序 */
    pos = L->length;
    for (i = 0; i < L->length; i++) {
        if (strcmp(L->data[i].id, d->id) > 0) {
            pos = i;
            break;
        }
    }

    return List_Insert(L, pos, d) ? 1 : 0;
}

int Service_RemoveById(SeqList* L, const char* id)
{
    int pos = List_FindById(L, id);

    if (pos < 0) return 0;
    return List_Delete(L, pos);
}

Doctor* Service_FindById(SeqList* L, const char* id)
{
    int pos;

    if (!L || !id) return NULL;
    pos = List_FindById(L, id);
    return (pos < 0) ? NULL : &L->data[pos];
}

void Service_FindByName(const SeqList* L, const char* name, IndexSet* out)
{
    int i;

    out->count = 0;
    if (!L || !name) return;

    for (i = 0; i < L->length; i++) {
        if (strstr(L->data[i].name, name) != NULL) {
            Set_Push(out, i);
        }
    }
}

/* ===================== 区间查找（筛选） ===================== */
void Service_SelectAll(const SeqList* L, IndexSet* out)
{
    int i;

    out->count = 0;
    if (!L) return;
    for (i = 0; i < L->length; i++) Set_Push(out, i);
}

void Service_SelectByIdRange(const SeqList* L, const char* lo, const char* hi, IndexSet* out)
{
    int i;

    out->count = 0;
    if (!L || !lo || !hi) return;

    for (i = 0; i < L->length; i++) {
        if (strcmp(L->data[i].id, lo) >= 0 && strcmp(L->data[i].id, hi) <= 0) {
            Set_Push(out, i);
        }
    }
}

void Service_SelectByBirthRange(const SeqList* L, const Date* lo, const Date* hi, IndexSet* out)
{
    int i;

    out->count = 0;
    if (!L || !lo || !hi) return;

    for (i = 0; i < L->length; i++) {
        if (Date_Compare(&L->data[i].birth, lo) >= 0 &&
            Date_Compare(&L->data[i].birth, hi) <= 0) {
            Set_Push(out, i);
        }
    }
}

void Service_SelectByAgeRange(const SeqList* L, int lo, int hi, IndexSet* out)
{
    int i;

    out->count = 0;
    if (!L) return;

    for (i = 0; i < L->length; i++) {
        int age = Date_Age(&L->data[i].birth);
        if (age >= lo && age <= hi) Set_Push(out, i);
    }
}

void Service_SelectByDept(const SeqList* L, Department dept, IndexSet* out)
{
    int i;

    out->count = 0;
    if (!L) return;

    for (i = 0; i < L->length; i++) {
        if (L->data[i].dept == dept) Set_Push(out, i);
    }
}

/* ===================== 区间修改 ===================== */
int Service_PromoteTitle(SeqList* L, const IndexSet* set)
{
    int i;
    int changed = 0;

    if (!L || !set) return 0;

    for (i = 0; i < set->count; i++) {
        Doctor* d = List_At(L, set->idx[i]);
        if (!d) continue;
        if (d->title < TITLE_CHIEF) {
            d->title = (Title)(d->title + 1);
            changed++;
        }
    }
    return changed;
}

int Service_PromoteEducation(SeqList* L, const IndexSet* set)
{
    int i;
    int changed = 0;

    if (!L || !set) return 0;

    for (i = 0; i < set->count; i++) {
        Doctor* d = List_At(L, set->idx[i]);
        if (!d) continue;
        if (d->education >= EDU_POSTDOC) continue;

        d->education = (Education)(d->education + 1);

        /* 学位与学历保持同步 */
        switch (d->education) {
            case EDU_COLLEGE:  d->degree = DEG_NONE;     break;
            case EDU_BACHELOR: d->degree = DEG_BACHELOR; break;
            case EDU_MASTER:   d->degree = DEG_MASTER;   break;
            default:           d->degree = DEG_PHD;      break;
        }
        changed++;
    }
    return changed;
}

int Service_ChangeDept(SeqList* L, const IndexSet* set, Department dept)
{
    int i;
    int changed = 0;

    if (!L || !set) return 0;

    for (i = 0; i < set->count; i++) {
        Doctor* d = List_At(L, set->idx[i]);
        if (!d || d->dept == dept) continue;
        d->dept = dept;
        changed++;
    }
    return changed;
}

/* ===================== 区间统计 ===================== */
void Service_StatsSet(const SeqList* L, const IndexSet* set, Stats* out)
{
    int i;

    memset(out, 0, sizeof *out);
    out->minAge = -1;
    out->maxAge = -1;

    if (!L || !set) return;

    for (i = 0; i < set->count; i++) {
        int idx = set->idx[i];
        const Doctor* d;
        int age;

        if (idx < 0 || idx >= L->length) continue;
        d = &L->data[idx];
        age = Date_Age(&d->birth);

        out->count++;
        out->ageSum += age;
        if (out->minAge < 0 || age < out->minAge) out->minAge = age;
        if (out->maxAge < 0 || age > out->maxAge) out->maxAge = age;

        if (d->gender    >= 0 && d->gender    < GENDER_COUNT) out->genderCount[d->gender]++;
        if (d->education >= 0 && d->education < EDU_COUNT)    out->eduCount[d->education]++;
        if (d->dept      >= 0 && d->dept      < DEPT_COUNT)   out->deptCount[d->dept]++;
        if (d->title     >= 0 && d->title     < TITLE_COUNT)  out->titleCount[d->title]++;
        if (d->field     >= 0 && d->field     < FIELD_COUNT)  out->fieldCount[d->field]++;
    }
}

static void PrintDistribution(const char* label, const char* const* names,
                              int count, const int* values, int total)
{
    int i;

    printf("  ");
    Text_PrintPad(label, 12);
    printf(":\n");

    for (i = 0; i < count; i++) {
        printf("      ");
        Text_PrintPad(names[i], 14);
        printf("%4d (%5.1f%%)\n", values[i],
               total > 0 ? 100.0 * values[i] / total : 0.0);
    }
}

void Service_PrintStats(const Stats* s)
{
    printf("\n================ 区间统计结果 ================\n");

    printf("  ");
    Text_PrintPad("总人数", 12);
    printf(": %d\n", s->count);

    if (s->count == 0) {
        printf("==============================================\n");
        return;
    }

    printf("  ");
    Text_PrintPad("平均年龄", 12);
    printf(": %.1f 岁\n", (double)s->ageSum / s->count);

    printf("  ");
    Text_PrintPad("年龄范围", 12);
    printf(": %d ~ %d 岁\n", s->minAge, s->maxAge);

    PrintDistribution("性别分布", GENDER_NAMES, GENDER_COUNT, s->genderCount, s->count);
    PrintDistribution("学历分布", EDU_NAMES,    EDU_COUNT,    s->eduCount,    s->count);
    PrintDistribution("职称分布", TITLE_NAMES,  TITLE_COUNT,  s->titleCount,  s->count);
    PrintDistribution("科室分布", DEPT_NAMES,   DEPT_COUNT,   s->deptCount,   s->count);
    PrintDistribution("研究领域", FIELD_NAMES,  FIELD_COUNT,  s->fieldCount,  s->count);

    printf("==============================================\n");
}

/* ===================== 排序 ===================== */
static int CmpById(const void* a, const void* b)
{
    return strcmp(((const Doctor*)a)->id, ((const Doctor*)b)->id);
}

static int CmpByName(const void* a, const void* b)
{
    int c = strcmp(((const Doctor*)a)->name, ((const Doctor*)b)->name);
    if (c != 0) return c;
    return strcmp(((const Doctor*)a)->id, ((const Doctor*)b)->id);   /* 姓名相同按工号稳定排序 */
}

static int CmpByAge(const void* a, const void* b)
{
    int aa = Date_Age(&((const Doctor*)a)->birth);
    int bb = Date_Age(&((const Doctor*)b)->birth);

    if (aa != bb) return aa - bb;
    return strcmp(((const Doctor*)a)->id, ((const Doctor*)b)->id);
}

void Service_SortById(SeqList* L)
{
    if (L && L->length > 1) qsort(L->data, (size_t)L->length, sizeof(Doctor), CmpById);
}

void Service_SortByName(SeqList* L)
{
    if (L && L->length > 1) qsort(L->data, (size_t)L->length, sizeof(Doctor), CmpByName);
}

void Service_SortByAge(SeqList* L)
{
    if (L && L->length > 1) qsort(L->data, (size_t)L->length, sizeof(Doctor), CmpByAge);
}

/* ===================== 文件读写 ===================== */
#define SAVE_FIELD_NUM 11   /* 每行保存的字段个数 */

int Service_Save(const SeqList* L, const char* path)
{
    FILE* fp;
    int i;

    if (!L || !path) return 0;

    fp = fopen(path, "w");
    if (!fp) return 0;

    fprintf(fp, "# 博士(医师)信息管理系统 数据文件\n");
    fprintf(fp, "# 字段顺序: name|id|gender|birth|education|degree|major|position|title|dept|field\n");
    fprintf(fp, "# 枚举字段以序号存储，对照表如下:\n");
    fprintf(fp, "#   gender   : 0=%s 1=%s\n", GENDER_NAMES[0], GENDER_NAMES[1]);
    fprintf(fp, "#   education: 0=%s 1=%s 2=%s 3=%s 4=%s\n",
            EDU_NAMES[0], EDU_NAMES[1], EDU_NAMES[2], EDU_NAMES[3], EDU_NAMES[4]);
    fprintf(fp, "#   degree   : 0=%s 1=%s 2=%s 3=%s\n",
            DEG_NAMES[0], DEG_NAMES[1], DEG_NAMES[2], DEG_NAMES[3]);
    fprintf(fp, "#   position : 0=%s 1=%s 2=%s 3=%s 4=%s\n",
            POS_NAMES[0], POS_NAMES[1], POS_NAMES[2], POS_NAMES[3], POS_NAMES[4]);
    fprintf(fp, "#   title    : 0=%s 1=%s 2=%s 3=%s\n",
            TITLE_NAMES[0], TITLE_NAMES[1], TITLE_NAMES[2], TITLE_NAMES[3]);
    fprintf(fp, "#   dept     : 0=%s 1=%s 2=%s 3=%s 4=%s 5=%s\n",
            DEPT_NAMES[0], DEPT_NAMES[1], DEPT_NAMES[2],
            DEPT_NAMES[3], DEPT_NAMES[4], DEPT_NAMES[5]);
    fprintf(fp, "#   field    : 0=%s 1=%s 2=%s 3=%s\n",
            FIELD_NAMES[0], FIELD_NAMES[1], FIELD_NAMES[2], FIELD_NAMES[3]);
    fprintf(fp, "# 记录数: %d\n", L->length);

    for (i = 0; i < L->length; i++) {
        const Doctor* d = &L->data[i];
        char birth[16];

        Date_ToStr(&d->birth, birth, sizeof birth);
        fprintf(fp, "%s|%s|%d|%s|%d|%d|%s|%d|%d|%d|%d\n",
                d->name, d->id, (int)d->gender, birth,
                (int)d->education, (int)d->degree, d->major,
                (int)d->position, (int)d->title, (int)d->dept, (int)d->field);
    }

    fclose(fp);
    return 1;
}

/* 按 '|' 切分一行，返回切分出的字段个数 */
static int SplitLine(char* line, char* fields[], int maxFields)
{
    int n = 0;
    char* p = line;

    while (n < maxFields) {
        char* sep;

        fields[n++] = p;
        sep = strchr(p, '|');
        if (!sep) break;
        *sep = '\0';
        p = sep + 1;
    }
    return n;
}

int Service_Load(SeqList* L, const char* path, int* skipped)
{
    FILE* fp;
    char  line[512];
    int   loaded = 0;
    int   bad = 0;

    if (skipped) *skipped = 0;
    if (!L || !path) return -1;

    fp = fopen(path, "r");
    if (!fp) return -1;

    List_Clear(L);

    while (fgets(line, sizeof line, fp)) {
        char* fields[SAVE_FIELD_NUM];
        Doctor d;
        char err[128];

        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;

        if (SplitLine(line, fields, SAVE_FIELD_NUM) != SAVE_FIELD_NUM) {
            bad++;
            continue;
        }

        memset(&d, 0, sizeof d);
        snprintf(d.name, NAME_LEN, "%s", fields[0]);
        if (!Doctor_SetId(&d, fields[1])) { bad++; continue; }

        d.gender    = (Gender)atoi(fields[2]);
        if (!Date_FromStr(fields[3], &d.birth)) { bad++; continue; }
        d.education = (Education)atoi(fields[4]);
        d.degree    = (Degree)atoi(fields[5]);
        snprintf(d.major, STR_LEN, "%s", fields[6]);
        d.position  = (Position)atoi(fields[7]);
        d.title     = (Title)atoi(fields[8]);
        d.dept      = (Department)atoi(fields[9]);
        d.field     = (Field)atoi(fields[10]);

        if (!Doctor_Validate(&d, err, sizeof err)) { bad++; continue; }

        if (Service_Add(L, &d) == 1) loaded++;
        else                         bad++;   /* 工号重复或内存不足 */
    }

    fclose(fp);
    if (skipped) *skipped = bad;
    return loaded;
}

int Service_Generate(SeqList* L, int count)
{
    int added = 0;
    int seq;
    int i;

    if (!L || count <= 0) return 0;

    seq = L->length + 1;

    for (i = 0; i < count; i++) {
        Doctor d;

        do {
            Doctor_Random(&d, seq++);
        } while (Service_FindById(L, d.id) != NULL);   /* 极端情况下避免工号撞号 */

        if (Service_Add(L, &d) == 1) added++;
    }
    return added;
}
