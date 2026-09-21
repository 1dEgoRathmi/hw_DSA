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

#define SAVE_FIELD_NUM 13   /* 数据文件每行的字段个数 */

/* ===================== 内部工具 ===================== */
static void Set_Push(IndexSet* out, int idx)
{
    if (out->count < MAX_RESULT) {
        out->idx[out->count++] = idx;
    }
}

/* ===================== 入职 / 离职 ===================== */
int Service_Hire(SeqList* L, Doctor* d, int* outSlot)
{
    int i;
    int vacant     = -1;   /* 最靠前的空位 */
    int sameWorkId = -1;   /* 工号与待入职者相同的空位（即离职老员工的栏位） */

    if (!L || !d) return HIRE_FAIL_MEMORY;

    /* 工号不接受手工输入，一律由 科室+领域+职称+姓+名 派生 */
    Doctor_BuildWorkId(d);
    d->active = SLOT_INUSE;

    for (i = 0; i < L->length; i++) {
        Doctor* p = &L->data[i];
        if (p->active == SLOT_INUSE) {
            if (strcmp(p->workId, d->workId) == 0) return HIRE_FAIL_DUP_WORKID;
            if (strcmp(p->idCard, d->idCard) == 0) return HIRE_FAIL_DUP_IDCARD;
        } else {
            if (vacant < 0) vacant = i;
            if (sameWorkId < 0 && strcmp(p->workId, d->workId) == 0) sameWorkId = i;
        }
    }

    /* 优先重用离职老员工的工号（落回其原栏位） */
    if (sameWorkId >= 0) {
        L->data[sameWorkId] = *d;
        if (outSlot) *outSlot = sameWorkId + 1;
        return HIRE_OK_REUSE_WORKID;
    }
    /* 其次按序复用最靠前的空位 */
    if (vacant >= 0) {
        L->data[vacant] = *d;
        if (outSlot) *outSlot = vacant + 1;
        return HIRE_OK_REUSE_SLOT;
    }
    /* 人员真实扩充，才真正开辟新栏位 */
    if (!List_Append(L, d)) return HIRE_FAIL_MEMORY;
    if (outSlot) *outSlot = L->length;
    return HIRE_OK_NEW_SLOT;
}

int Service_Resign(SeqList* L, const char* workId)
{
    int pos = Service_FindIndexByWorkId(L, workId);

    if (pos < 0) return 0;
    /* 只切换不外显标识符，不做任何前移，后续工号保持原样 */
    L->data[pos].active = SLOT_VACANT;
    return 1;
}

int Service_PurgeVacant(SeqList* L)
{
    int i;
    int n = 0;

    if (!L) return 0;
    /* 从后往前删除，避免删除后下标错位 */
    for (i = L->length - 1; i >= 0; i--) {
        if (L->data[i].active == SLOT_VACANT) {
            List_Delete(L, i);
            n++;
        }
    }
    return n;
}

/* ===================== 计数 ===================== */
int Service_ActiveCount(const SeqList* L)
{
    int i;
    int n = 0;

    if (!L) return 0;
    for (i = 0; i < L->length; i++) {
        if (L->data[i].active == SLOT_INUSE) n++;
    }
    return n;
}

int Service_VacantCount(const SeqList* L)
{
    if (!L) return 0;
    return L->length - Service_ActiveCount(L);
}

/* ===================== 查找 ===================== */
int Service_FindIndexByWorkId(const SeqList* L, const char* workId)
{
    int i;

    if (!L || !workId) return -1;
    for (i = 0; i < L->length; i++) {
        if (L->data[i].active == SLOT_INUSE &&
            strcmp(L->data[i].workId, workId) == 0) {
            return i;
        }
    }
    return -1;
}

int Service_FindIndexByIdCard(const SeqList* L, const char* idCard)
{
    int i;

    if (!L || !idCard) return -1;
    for (i = 0; i < L->length; i++) {
        if (L->data[i].active == SLOT_INUSE &&
            strcmp(L->data[i].idCard, idCard) == 0) {
            return i;
        }
    }
    return -1;
}

Doctor* Service_FindByWorkId(SeqList* L, const char* workId)
{
    int pos = Service_FindIndexByWorkId(L, workId);

    return (pos < 0) ? NULL : &L->data[pos];
}

/* ===================== 复合检索 ===================== */
void Query_Init(Query* q)
{
    memset(q, 0, sizeof *q);
    q->activeFilter = -1;   /* 默认只检索在岗人员 */
}

static int MatchQuery(const Doctor* d, const Query* q)
{
    if (q->activeFilter == -1 && d->active != SLOT_INUSE) return 0;
    if (q->activeFilter ==  1 && d->active != SLOT_VACANT) return 0;

    if (q->useWorkId && strcmp(d->workId, q->workId) != 0) return 0;
    if (q->useWorkIdRange) {
        if (strcmp(d->workId, q->workIdLo) < 0) return 0;
        if (strcmp(d->workId, q->workIdHi) > 0) return 0;
    }
    if (q->useName   && strstr(d->name, q->name) == NULL)  return 0;
    if (q->useIdCard && strcmp(d->idCard, q->idCard) != 0) return 0;
    if (q->useGender && d->gender != q->gender)            return 0;

    if (q->useAge) {
        int age = Date_Age(&d->birth);
        if (age < q->ageLo || age > q->ageHi) return 0;
    }
    if (q->useBirth) {
        if (Date_Compare(&d->birth, &q->birthLo) < 0) return 0;
        if (Date_Compare(&d->birth, &q->birthHi) > 0) return 0;
    }

    if (q->useEdu      && d->education != q->edu)          return 0;
    if (q->useDegree   && d->degree    != q->degree)       return 0;
    if (q->useMajor    && strstr(d->major, q->major) == NULL) return 0;
    if (q->usePosition && d->position  != q->position)     return 0;
    if (q->useTitle    && d->title     != q->title)        return 0;
    if (q->useDept     && d->dept      != q->dept)         return 0;
    if (q->useField    && d->field     != q->field)        return 0;

    return 1;
}

void Service_Query(const SeqList* L, const Query* q, IndexSet* out)
{
    int i;

    out->count = 0;
    if (!L || !q) return;

    for (i = 0; i < L->length; i++) {
        if (MatchQuery(&L->data[i], q)) Set_Push(out, i);
    }
}

/* ===================== 区间查找 ===================== */
void Service_SelectAll(const SeqList* L, IndexSet* out)
{
    Query q;

    Query_Init(&q);
    Service_Query(L, &q, out);
}

void Service_SelectByWorkIdRange(const SeqList* L, const char* lo, const char* hi, IndexSet* out)
{
    int i;

    out->count = 0;
    if (!L || !lo || !hi) return;

    for (i = 0; i < L->length; i++) {
        const Doctor* d = &L->data[i];
        if (d->active != SLOT_INUSE) continue;
        if (strcmp(d->workId, lo) >= 0 && strcmp(d->workId, hi) <= 0) Set_Push(out, i);
    }
}

void Service_SelectByBirthRange(const SeqList* L, const Date* lo, const Date* hi, IndexSet* out)
{
    int i;

    out->count = 0;
    if (!L || !lo || !hi) return;

    for (i = 0; i < L->length; i++) {
        const Doctor* d = &L->data[i];
        if (d->active != SLOT_INUSE) continue;
        if (Date_Compare(&d->birth, lo) >= 0 && Date_Compare(&d->birth, hi) <= 0) Set_Push(out, i);
    }
}

void Service_SelectByAgeRange(const SeqList* L, int lo, int hi, IndexSet* out)
{
    Query q;

    Query_Init(&q);
    q.useAge = 1;
    q.ageLo = lo;
    q.ageHi = hi;
    Service_Query(L, &q, out);
}

void Service_SelectByDept(const SeqList* L, Department dept, IndexSet* out)
{
    Query q;

    Query_Init(&q);
    q.useDept = 1;
    q.dept = dept;
    Service_Query(L, &q, out);
}

/* ===================== 工号重算 ===================== */
int Service_RebuildWorkId(SeqList* L, int idx)
{
    char   newId[WORKID_LEN];
    Doctor tmp;
    int    i;

    if (!L || idx < 0 || idx >= L->length) return 0;

    tmp = L->data[idx];
    Doctor_BuildWorkId(&tmp);

    if (strcmp(tmp.workId, L->data[idx].workId) == 0) return 1;   /* 工号无变化 */

    /* 新工号不能与其他在岗人员冲突 */
    for (i = 0; i < L->length; i++) {
        if (i == idx) continue;
        if (L->data[i].active == SLOT_INUSE &&
            strcmp(L->data[i].workId, tmp.workId) == 0) {
            return 0;
        }
    }

    snprintf(newId, WORKID_LEN, "%s", tmp.workId);
    snprintf(L->data[idx].workId, WORKID_LEN, "%s", newId);
    return 1;
}

/* ===================== 区间修改 ===================== */
int Service_PromoteTitle(SeqList* L, const IndexSet* set, int* conflict)
{
    int i;
    int changed = 0;
    int bad = 0;

    if (!L || !set) return 0;

    for (i = 0; i < set->count; i++) {
        Doctor* d = List_At(L, set->idx[i]);

        if (!d || d->active != SLOT_INUSE) continue;
        if (d->title >= TITLE_CHIEF) continue;          /* 已到顶 */

        d->title = (Title)(d->title + 1);
        if (Service_RebuildWorkId(L, set->idx[i])) {
            changed++;
        } else {
            d->title = (Title)(d->title - 1);           /* 工号冲突，回滚 */
            bad++;
        }
    }

    if (conflict) *conflict = bad;
    return changed;
}

int Service_PromoteEducation(SeqList* L, const IndexSet* set, int* conflict)
{
    int i;
    int changed = 0;

    if (!L || !set) return 0;

    for (i = 0; i < set->count; i++) {
        Doctor* d = List_At(L, set->idx[i]);

        if (!d || d->active != SLOT_INUSE) continue;
        if (d->education >= EDU_POSTDOC) continue;

        /* 学历不参与工号构成，无需重算 */
        d->education = (Education)(d->education + 1);
        switch (d->education) {
            case EDU_COLLEGE:  d->degree = DEG_NONE;     break;
            case EDU_BACHELOR: d->degree = DEG_BACHELOR; break;
            case EDU_MASTER:   d->degree = DEG_MASTER;   break;
            default:           d->degree = DEG_PHD;      break;
        }
        changed++;
    }

    if (conflict) *conflict = 0;
    return changed;
}

int Service_ChangeDept(SeqList* L, const IndexSet* set, Department dept, int* conflict)
{
    int i;
    int changed = 0;
    int bad = 0;

    if (!L || !set) return 0;

    for (i = 0; i < set->count; i++) {
        Doctor* d = List_At(L, set->idx[i]);

        if (!d || d->active != SLOT_INUSE) continue;
        if (d->dept == dept) continue;

        d->dept = dept;
        if (Service_RebuildWorkId(L, set->idx[i])) {
            changed++;
        } else {
            bad++;
        }
    }

    if (conflict) *conflict = bad;
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

/* ===================== 结果集排序 ===================== */
static const SeqList* g_sortList = NULL;
static SortKey        g_sortKey  = SORT_BY_WORKID;

static int CmpIndex(const void* a, const void* b)
{
    const Doctor* da = &g_sortList->data[*(const int*)a];
    const Doctor* db = &g_sortList->data[*(const int*)b];
    int c;

    switch (g_sortKey) {
    case SORT_BY_NAME:
        c = strcmp(da->name, db->name);
        if (c != 0) return c;
        return strcmp(da->workId, db->workId);
    case SORT_BY_AGE:
        c = Date_Age(&da->birth) - Date_Age(&db->birth);
        if (c != 0) return c;
        return strcmp(da->workId, db->workId);
    default:
        return strcmp(da->workId, db->workId);
    }
}

void Service_SortSet(const SeqList* L, IndexSet* set, SortKey key)
{
    if (!L || !set || set->count < 2) return;

    g_sortList = L;
    g_sortKey  = key;
    qsort(set->idx, (size_t)set->count, sizeof(int), CmpIndex);
    g_sortList = NULL;
}

/* ===================== 文件读写 ===================== */
int Service_Save(const SeqList* L, const char* path)
{
    FILE* fp;
    int   i;

    if (!L || !path) return 0;

    fp = fopen(path, "w");
    if (!fp) return 0;

    fprintf(fp, "# 博士(医师)信息管理系统 数据文件  v2.1\n");
    fprintf(fp, "# 字段顺序: workId|idCard|active|name|gender|birth|education|degree|major|position|title|dept|field\n");
    fprintf(fp, "# 工号 workId 为 8 位结构化编号: 科室(1) 领域(1) 职称(1) 姓(2) 名(3)\n");
    fprintf(fp, "#   active 为不外显标识符: %d=在岗, %d=空位(已离职，工号可被重用)\n",
            SLOT_INUSE, SLOT_VACANT);
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
    fprintf(fp, "# 栏位总数: %d, 在岗: %d, 空位: %d\n",
            L->length, Service_ActiveCount(L), Service_VacantCount(L));

    for (i = 0; i < L->length; i++) {
        const Doctor* d = &L->data[i];
        char birth[16];

        Date_ToStr(&d->birth, birth, sizeof birth);
        fprintf(fp, "%s|%s|%d|%s|%d|%s|%d|%d|%s|%d|%d|%d|%d\n",
                d->workId, d->idCard, d->active, d->name, (int)d->gender, birth,
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
        char*  fields[SAVE_FIELD_NUM];
        Doctor d;
        char   err[128];
        int    dup = 0;
        int    i;

        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;

        if (SplitLine(line, fields, SAVE_FIELD_NUM) != SAVE_FIELD_NUM) {
            bad++;
            continue;
        }

        memset(&d, 0, sizeof d);
        snprintf(d.name, NAME_LEN, "%s", fields[3]);

        if (!Doctor_SetIdCard(&d, fields[1])) { bad++; continue; }

        d.active    = atoi(fields[2]);
        d.education = (Education)atoi(fields[6]);
        d.degree    = (Degree)atoi(fields[7]);
        snprintf(d.major, STR_LEN, "%s", fields[8]);
        d.position  = (Position)atoi(fields[9]);
        d.title     = (Title)atoi(fields[10]);
        d.dept      = (Department)atoi(fields[11]);
        d.field     = (Field)atoi(fields[12]);

        /* 性别与出生日期以身份证号为准重新派生，保证数据自洽 */
        if (!Date_FromStr(fields[5], &d.birth)) { bad++; continue; }

        /* 工号不信任文件内容，按属性重算，保证「工号 <=> 属性」不变式成立 */
        Doctor_BuildWorkId(&d);

        if (!Doctor_Validate(&d, err, sizeof err)) { bad++; continue; }

        /* 在岗记录之间不允许工号或身份证号重复 */
        if (d.active == SLOT_INUSE) {
            for (i = 0; i < L->length; i++) {
                if (L->data[i].active != SLOT_INUSE) continue;
                if (strcmp(L->data[i].workId, d.workId) == 0 ||
                    strcmp(L->data[i].idCard, d.idCard) == 0) {
                    dup = 1;
                    break;
                }
            }
        }
        if (dup) { bad++; continue; }

        if (!List_Append(L, &d)) { bad++; continue; }
        loaded++;
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
        int    rc = HIRE_FAIL_DUP_WORKID;
        int    tries = 0;

        /* 随机属性可能派生出相同工号，撞号时重新生成 */
        while (rc < 0 && tries < 32) {
            Doctor_Random(&d, seq++);
            rc = Service_Hire(L, &d, NULL);
            tries++;
        }
        if (rc > 0) added++;
    }
    return added;
}
