# 博士（医师）信息管理系统

> 数据结构课程作业 · 线性表（顺序表）综合应用
> 语言：C（C99） · 平台：Windows / MinGW-W64 gcc · 控制台程序

---

## 一、作业背景与题目

本程序对应课程作业题目「结构体线性表」。

**初始版本（v1.0）完整保留在本仓库的 git 首次提交中**，可随时查看：

```bash
git show 6778ccc --stat        # 查看初始版本改动了哪些文件
git show 6778ccc:doctor.h      # 查看初始版本的数据模型
git show 6778ccc:service.c     # 查看初始版本的业务层实现
```

作业基本要求：

1. 用**顺序表**存储 100 条以上「博士（医师）」信息记录；
2. 每条记录包含姓名、编号、性别、出生日期、学历、学位、专业、职务、职称、科室、研究领域等字段；
3. 实现**增、删、查、改**四类基本操作；
4. 进一步实现**区间查找、区间修改、区间统计**等较复杂的操作；
5. 数据可保存到文件、可从文件读回。

在此基础之上，先后进行了两轮修改：

- **v2.0**：按 5 条新需求对数据模型与业务层做了重构（工号/身份证号分离、空位复用、双模式显示、复合检索）；
- **v2.1**：在 v2.0 之上做交互与结构精简（中途退出机制、简略模式去掉出生日期、取消独立区间检索并把工号区间并入常规检索、结果排序与「全部记录」集成进检索、统一的分页浏览按键）。

两轮修改的详细记录见 [第六节 版本记录](#六版本记录)。

---

## 二、编译与运行

### 2.1 编译

在项目目录下执行：

```bash
gcc -std=c99 -Wall -Wextra -O2 doctor.c seqlist.c service.c main.c -o doctor.exe
```

- 要求 gcc 支持 `-std=c99`（实测 MinGW-W64 gcc 14.2.0）；
- 在 `-Wall -Wextra` 下**零警告、零错误**；
- Windows 控制台需要 UTF-8 支持，程序已在启动时调用
  `SetConsoleOutputCP(65001)` / `SetConsoleCP(65001)` 处理中文显示。

### 2.2 运行

```bash
doctor.exe            # 启动交互式菜单
doctor.exe -help      # 显示帮助并退出（-h / --help 等效）
```

程序启动时自动读取同目录下的 `data.txt`：

| 情况 | 程序行为 |
| --- | --- |
| 文件不存在 | 随机生成 120 条测试数据 |
| 文件存在但**全部记录格式不符**（例如旧版本文件） | 提示后重新生成 120 条测试数据 |
| 文件存在且格式正确 | 加载记录，并显示「在岗 / 空位」统计 |

退出时自动把当前数据写回 `data.txt`。

---

## 三、目录结构与分层架构

程序采用**四层架构**，每层只依赖其下一层，便于单独理解与替换：

```
┌──────────────────────────────────────────────┐
│  交互层  main.c       菜单、输入校验、分页输出   │
├──────────────────────────────────────────────┤
│  业务层  service.c/h  入职/离职、复合检索、统计  │
├──────────────────────────────────────────────┤
│  存储层  seqlist.c/h  顺序表：扩容、插入、删除   │
├──────────────────────────────────────────────┤
│  数据层  doctor.c/h   实体定义、工号/身份证规则  │
└──────────────────────────────────────────────┘
```

| 文件 | 层次 | 职责 |
| --- | --- | --- |
| [doctor.h](doctor.h) / [doctor.c](doctor.c) | 数据层 | `Doctor` 实体、枚举与中文映射表、日期工具、UTF-8 显示宽度对齐、**8 位工号派生**、**18 位身份证号校验与派生**、双模式表格输出（简略模式不含出生日期） |
| [seqlist.h](seqlist.h) / [seqlist.c](seqlist.c) | 存储层 | 顺序表（动态数组）：`realloc` 倍增扩容、`memmove` 插入删除、按下标/工号查找 |
| [service.h](service.h) / [service.c](service.c) | 业务层 | 入职（空位复用）、离职（逻辑删除）、复合检索（**含工号区间**）、区间修改/统计、结果集排序、文件读写 |
| [main.c](main.c) | 交互层 | 主菜单、复合条件编辑界面、结果集分页浏览（简略/详细切换、排序）、**中途退出（`q` 键）**、`-help` |
| [data.txt](data.txt) | 数据 | 以 `\|` 分隔的纯文本数据文件，枚举字段以序号存储 |
| git 提交 `6778ccc` | 存档 | **初始版本（v1.0）的全部源码**，保留未删除 |

---

## 四、数据结构设计

### 4.1 实体 `Doctor`

```c
typedef struct {
    char       workId[9];    /* 8 位结构化工号，由属性派生 */
    char       idCard[19];   /* 18 位身份证号 */
    int        active;       /* 不外显标识符：1=在岗, 0=空位 */
    char       name[32];     /* 姓名 */
    Gender     gender;       /* 性别（由身份证号派生） */
    Date       birth;        /* 出生日期（由身份证号派生） */
    Education  education;    /* 学历 */
    Degree     degree;       /* 学位 */
    char       major[64];    /* 专业 */
    Position   position;     /* 职务 */
    Title      title;        /* 职称 */
    Department dept;         /* 科室 */
    Field      field;        /* 研究领域 */
} Doctor;
```

枚举型字段与中文名称表一一对应，统一通过 `Enum_Name` / `Enum_Parse` 转换，
因此**界面与文件都用序号存储**，名称只用于显示。

| 枚举 | 取值 |
| --- | --- |
| `Gender` | 男、女 |
| `Education` | 大专、本科、硕士、博士、博士后 |
| `Degree` | 无、学士、硕士、博士 |
| `Position` | 医师、副主任、主任、副院长、院长 |
| `Title` | 住院医师、主治医师、副主任医师、主任医师 |
| `Department` | 内科、外科、妇产科、儿科、急诊科、影像科 |
| `Field` | 临床、科研、教学、管理 |

### 4.2 顺序表 `SeqList`

```c
typedef struct {
    Doctor* data;      /* 连续存储区，每个元素是一个「栏位」 */
    int     length;    /* 已开辟的栏位总数（含空位） */
    int     capacity;  /* 当前容量 */
} SeqList;
```

- 初始容量 16，`List_Append` 时容量不足则按 **2 倍** 增长（`realloc`）；
- `List_Insert` / `List_Delete` 使用 `memmove` 完成元素搬移；
- 时间复杂度：按下标访问 O(1)，插入/删除 O(n)，顺序查找 O(n)。

### 4.3 8 位结构化工号（与身份证号彻底分离）

工号是**内部管理编号**，不是身份标识，由属性**自动派生**，不接受手工输入：

```
 4 0 1 0 7 9 1 6
 │ │ │ └─┬─┘ └─┬──┘
 │ │ │   │      └── 名（3 位，名称摘要码，1~999）
 │ │ │   └───────── 姓（2 位，姓氏表序号，1~60）
 │ │ └───────────── 职称（1 位，Title 枚举值）
 │ └─────────────── 研究领域（1 位，Field 枚举值）
 └───────────────── 科室（1 位，Department 枚举值）
```

- 姓 / 名的编码：姓名先按 UTF-8 切成「姓」与「名」，姓查姓氏表（60 个常见姓）
  得序号，名用 djb2 哈希映射到 1~999；
- 派生函数 `Doctor_BuildWorkId`，解析函数 `Doctor_ExplainWorkId`（界面上会显示
  「科室4-领域0-职称0-姓31-名814」这样的解释）；
- **不变式**：`Doctor_Validate` 会重新派生一次工号并与记录中的工号比对，
  不一致即判定记录非法，从而保证「工号 ⇔ 属性」始终一致；
- 职称、科室变更后由 `Service_RebuildWorkId` 自动重算；若重算结果与在岗人员冲突，
  则**回滚**该条修改。

### 4.4 18 位身份证号

- 校验：前 17 位必须为数字，末位为数字或 `X`；按加权因子
  `{7,9,10,5,8,4,2,1,6,3,7,9,10,5,8,4,2}` 求加权和后对 11 取模，与校验码表
  `{1,0,X,9,8,7,6,5,4,3,2}` 比对；
- **派生**：由第 7~14 位得到出生日期，由第 17 位（顺序码末位）奇偶得到性别
  （奇数为男、偶数为女）。因此录入时无需重复输入性别与出生日期。

### 4.5 不外显标识符 `active`

```c
#define SLOT_VACANT 0   /* 空位：原人员已离职，工号可被重用 */
#define SLOT_INUSE  1   /* 在岗：工号被当前人员占用 */
```

`active` 只用于内部控制栏位是否被占用，**不出现在任何人员信息表中**，
也不会显示给用户，是纯粹的内部实现细节。

---

## 五、核心实现要点

### 5.1 入职的三级落位策略

新员工入职时按顺序尝试三种落位方式，避免无意义的栏位膨胀：

| 优先级 | 结果常量 | 条件 | 行为 |
| --- | --- | --- | --- |
| 1 | `HIRE_OK_REUSE_WORKID` | 存在**空位**且其工号与新员工派生出的工号相同 | 落回该离职老员工的原栏位 |
| 2 | `HIRE_OK_REUSE_SLOT` | 存在任意空位 | 复用**最靠前**的空位栏位 |
| 3 | `HIRE_OK_NEW_SLOT` | 没有空位 | 末尾**真实开辟**新栏位（人员真实扩充） |

失败情形：`HIRE_FAIL_DUP_WORKID`（派生工号已被在岗人员占用）、
`HIRE_FAIL_DUP_IDCARD`（身份证号重复）、`HIRE_FAIL_MEMORY`。

### 5.2 离职：逻辑删除，后续工号不前移

`Service_Resign` 只把目标栏位的 `active` 置为 `SLOT_VACANT`，**不做任何元素搬移**，
因此其他人员的工号与相对次序完全不变。被释放的工号可以被新员工重用。

如果确实需要物理回收空间，可显式调用 `Service_PurgeVacant`（菜单 7-4），
它**从后往前**删除空位以避免下标错位。

### 5.3 复合检索

`Query` 结构体为每个字段配一个 `useXxx` 开关，多个条件之间是**「与」关系**：

```c
typedef struct {
    int  useWorkId; char workId[9];      /* 工号精确 */
    int  useWorkIdRange;                 /* 工号区间（v2.1 由独立检索并入） */
         char workIdLo[9], workIdHi[9];
    int  useName;   char name[32];       /* 姓名模糊 */
    int  useIdCard; char idCard[19];     /* 身份证号精确 */
    int  useGender; Gender gender;       /* 性别 */
    int  useAge;    int ageLo, ageHi;    /* 年龄区间 */
    int  useBirth;  Date birthLo, birthHi; /* 出生日期区间 */
    int  useEdu;    Education edu;       /* 学历 */
    int  useDegree; Degree degree;       /* 学位 */
    int  useMajor;  char major[64];      /* 专业模糊 */
    int  usePosition; Position position; /* 职务 */
    int  useTitle;  Title title;         /* 职称 */
    int  useDept;   Department dept;     /* 科室 */
    int  useField;  Field field;         /* 研究领域 */
    int  activeFilter;                   /* -1 仅在职 / 0 全部 / 1 仅离职 */
} Query;
```

共 **15 项可检索条件**（条件编辑菜单另设「16. 清除全部条件」，故界面上显示 16 项）。
姓名、专业为**模糊匹配**（子串），其余为精确/区间匹配。工号区间为 v2.1 新增，
原先独立的「区间检索」菜单项已取消，其功能全部并入此处。

检索结果用 `IndexSet` 保存**命中的栏位下标**而不是记录副本，
因此结果集不占额外存储，且不会打乱顺序表的物理布局。

**「全部记录」的等价实现**：不设置任何具体条件直接执行检索，即按人员状态
（默认「在职 + 离职」）列出全部栏位，因此不再需要单独的「显示全部」菜单项。

### 5.4 结果集排序

排序只调整 `IndexSet.idx` 的次序（`qsort` + 静态比较上下文），
**不移动物理栏位**，以免破坏「空位复用」的语义。支持按工号 / 姓名 / 年龄升序。

v2.1 起，排序已**集成进结果浏览界面**：按 `o` 键即时选择排序键并重排当前结果集，
排序后游标回到首页。原先独立的「结果排序」菜单项已取消。

### 5.5 双模式显示

| 模式 | 列 |
| --- | --- |
| **简略模式** | 序号、工号、姓名、性别、年龄、职务、职称、科室、研究领域 |
| **详细模式** | 在简略模式基础上增加 **出生日期、身份证号、学历、学位、专业** |

在结果集浏览界面输入 `s` / `d` 即可随时切换。
**v2.1 起简略模式不再显示出生日期**（与身份证号同属较敏感/冗余信息，
需要时切到详细模式即可看到）。

### 5.6 中文表格对齐

`Text_DisplayWidth` 按 UTF-8 编码规则把中文（3 字节）计为 2 个显示宽度，
`Text_PrintPad` 据此补空格，从而在等宽控制台里对齐中英文混排的表格。

### 5.7 数据文件格式

```
# 字段顺序: workId|idCard|active|name|gender|birth|education|degree|major|position|title|dept|field
40107916|110037199512031134|1|黄子丽|0|1995-12-03|2|2|影像医学与核医学|0|1|4|0
```

共 13 个字段，以 `|` 分隔，`#` 开头为注释行。加载时**不信任文件中的工号**，
一律按属性重新派生，以保证数据自洽。

### 5.8 分页浏览与中途退出（v2.1）

**分页浏览**：所有结果集（信息检索、信息修改的定位、区间修改的前后对照）
统一由 `BrowseResultSet` 输出，每页 `PAGE_SIZE` 条，底部提示：

```
===== 已显示 10/22 条 =====
  [s] 简略模式  [d] 详细模式  [o] 排序   [回车] 继续  [q] 退出:
```

| 按键 | 行为 |
| --- | --- |
| `s` / `d` | 切换简略 / 详细模式 |
| `o` | 选择排序键，对结果集重新排序并把游标复位到首页 |
| 回车 | 显示下一页；已是末页则提示「已显示全部记录」并结束 |
| `q` | 退出浏览，返回上一层 |

**中途退出**：全局标志 `g_cancelled` 配合输入函数 `ReadLine` 实现——
**任意输入处输入 `q`（单独一个字母 q）即放弃当前操作**，由主循环统一提示
「已放弃当前操作，返回主菜单」并继续；主菜单选择处的 `q` 只被忽略，不会退出程序。
菜单内的选择处输入 `0` 则表示取消/返回上一层。

---

## 六、版本记录

### 6.1 初始版本 v1.0（作业基础要求）

> 源码位置：git 提交 `6778ccc`（Initial commit），可用 `git show 6778ccc:<文件名>` 查看。

- **数据模型**：`Doctor { name, id, gender, birth, education, degree, major, position, title, dept, field }`。
  其中 `id` 是 18 位数字编号，**既充当工号又充当身份标识**，二者没有区分。
- **删除**：`Service_RemoveById` → `List_Delete`，**物理删除**，被删元素之后的记录整体前移。
- **查找**：按工号精确查找、按姓名模糊查找、显示全部；
  区间查找支持工号区间 / 出生日期区间 / 年龄区间 / 科室。
- **显示**：只有一种表格（`Doctor_PrintHeader` / `Doctor_PrintRow`），
  身份证号、学历、学位、专业全部无条件显示。
- **排序**：`Service_SortById/ByName/ByAge` **直接对顺序表物理排序**。
- **文件格式**：11 字段 `name|id|gender|birth|education|degree|major|position|title|dept|field`。
- **实测记录**：120 条数据，插入/修改/查找/区间查找/区间修改/统计/排序/文件读写/`-help`
  全部通过；例如区间统计得 38 人、平均 36.7 岁。

**v1.0 暴露出的问题：**

1. 工号与身份证号混用，工号需要人工输入，容易与属性不一致；
2. 物理删除导致**后续记录前移**，工号跟着变化，外部引用（如纸质档案上的编号）失效；
3. 删除后新插入记录会追加到末尾，无法利用已释放的位置；
4. 只有一种显示模式，敏感字段（身份证号）无法隐藏；
5. 查找条件固定，无法组合，可检索项少。

### 6.2 本次修改 v2.0（对应 5 条新需求）

| 需求 | 落地方式 | 涉及代码 |
| --- | --- | --- |
| **0. 创建 README，保留初始版本与本次修改记录** | 本文件记录 v2.0 的实现与结构、v1.0 与 v2.0 的完整版本记录；v1.0 源码保留在 git 首次提交 `6778ccc` | `README.md` |
| **1. 分离工号与身份证号，固定 8 位工号映射为科室-领域-职称-姓名** | 新增 `workId`（8 位派生）与 `idCard`（18 位）两个独立字段；`Doctor_BuildWorkId` 派生、`Doctor_ExplainWorkId` 解释、`Doctor_Validate` 强校验一致性 | [doctor.h](doctor.h) / [doctor.c](doctor.c) |
| **2. 用不外显标识符控制工号是否使用；删除不前移；插入免输工号、按序检索空位、重用离职工号、真实扩充才开新栏位** | 新增 `active`（`SLOT_INUSE`/`SLOT_VACANT`）；`Service_Resign` 只切标识符；`Service_Hire` 三级落位（重用工号 → 复用空位 → 真实扩充）；`Service_PurgeVacant` 供显式物理清理 | [service.h](service.h) / [service.c](service.c) |
| **3. 两种查询的结果表都可切换详细/简略模式，简略不显示身份证号、学历、学位、专业** | `Doctor_PrintHeaderBrief/RowBrief` 与 `Doctor_PrintHeaderFull/RowFull` 两套输出；结果浏览界面提供 `s`/`d` 切换 | [doctor.c](doctor.c) / [main.c](main.c) |
| **4. 两种查询都支持复合检索，并增加可检索项** | 引入 `Query` 结构体与 `MatchQuery`（多条件「与」组合）；`EditQuery` 交互式条件编辑器；区间检索也可在其上叠加复合条件 | [service.h](service.h) / [service.c](service.c) / [main.c](main.c) |
| **5. 给出更符合业界真实身份信息管理系统的修改意见（含数据库连接，非 IAM）** | 见第七节 | 本文件 |

**v2.0 的行为变化对照：**

| 场景 | v1.0 | v2.0 |
| --- | --- | --- |
| 工号来源 | 人工输入 18 位编号 | 由科室+领域+职称+姓名自动派生 8 位 |
| 身份标识 | 与工号混用 | 独立 18 位身份证号（含校验位） |
| 性别 / 出生日期 | 手工录入 | 由身份证号自动派生 |
| 删除 | 物理删除，后续记录前移 | 只切 `active` 标识符，**不前移** |
| 插入落位 | 追加到末尾 | 重用工号 → 复用空位 → 真实扩充 |
| 结果表显示 | 单一模式 | 简略 / 详细可切换 |
| 检索条件 | 固定几种 | 多项条件自由组合（「与」） |
| 排序 | 物理排序整个顺序表 | 只排序结果集下标，不动物理栏位 |
| 数据文件 | 11 字段 | 13 字段（含 `workId`、`active`） |

### 6.3 本次修改 v2.1（交互与结构精简）

v2.1 在 v2.0 之上做了一轮交互与结构精简，对应 5 条新要求：

| 需求 | 落地方式 | 涉及代码 |
| --- | --- | --- |
| **1. 设置中途退出机制：增删改查在输入、选择时都可退出** | 新增全局标志 `g_cancelled`；`ReadLine` 检测到单独输入 `q` 即置位并放弃当前操作；主循环统一提示后返回主菜单（主菜单处的 `q` 只忽略） | [main.c](main.c) |
| **2. 简略模式不显示生日** | `Doctor_PrintHeaderBrief` / `Doctor_PrintRowBrief` 删除「出生日期」列；`TABLE_WIDTH_BRIEF` 由 88 调整为 76 | [doctor.c](doctor.c) |
| **3. 取消独立区间检索，把工号区间并入常规检索；冗余的结果排序、全部记录模块一并取消** | `Query` 新增 `useWorkIdRange` / `workIdLo` / `workIdHi`，`MatchQuery` 增加区间判定；删除 `HandleRangeQuery`、`FilterSetByWorkIdRange`、`HandleSort`；「全部记录」用空条件检索等价实现；结果排序改为结果浏览界面的 `o` 键 | [service.h](service.h) / [service.c](service.c) / [main.c](main.c) |
| **4. 查询结果改统一的分页浏览按键（`[s]`/`[d]`/`[o]`/回车/`[q]`）** | `ShowResultSet` 重写为 `BrowseResultSet`（分页状态机），所有结果集共用；`PrintSetPaged` 改为无标题的 `PrintSetPage` | [main.c](main.c) |
| **5. 核查 doctor.c 中创建/销毁顺序表的功能是否未使用** | 经检索确认 `List_Create` / `List_Destroy` 在 [main.c](main.c) 中**均被调用**（分别在第 927 行、第 988 行），并非死代码 | [main.c](main.c) / [seqlist.c](seqlist.c) |

**v2.1 的行为变化对照：**

| 场景 | v2.0 | v2.1 |
| --- | --- | --- |
| 中途退出 | 只能逐级返回，`q` 无特殊含义 | 任意输入处输入 `q` 即放弃当前操作返回主菜单 |
| 简略模式列 | 含出生日期 | **不含出生日期**（需要时切详细模式） |
| 工号区间检索 | 独立的「区间检索」菜单 | 并入常规检索的条件 2 |
| 全部记录 | 独立的菜单项 | 空条件检索等价实现 |
| 结果排序 | 独立的「结果排序」菜单 | 结果浏览界面按 `o` |
| 结果浏览提示 | 逐条/翻页的旧提示 | 统一的 `[s][d][o][回车][q]` 提示行 |
| 主菜单编号 | 1 入职 … 多级子菜单 | 1 入职 2 离职 3 信息检索 4 信息修改 5 区间修改 6 区间统计 7 文件与数据 |

### 6.4 回归测试记录

在 `-Wall -Wextra` 零警告下重新编译后，实测结果：

| 测试项 | 结果 |
| --- | --- |
| `doctor.exe -help` | 正常输出 v2.1 帮助（含 16 项可检索项、分页按键说明） |
| 启动加载旧版 11 字段 `data.txt` | 提示「120 条记录格式不符（可能为旧版本），已重新生成 120 条」 |
| 复合检索（科室=内科） | 命中 17 条，简略模式 9 列（**已无出生日期**） |
| 结果表切换详细模式（`d`） | 表头增加出生日期、身份证号、学历、学位、专业 |
| 工号区间检索（并入常规检索条件 2） | 命中 22 条，分页提示符为 `[s] [d] [o] [回车] [q]` |
| 结果集排序（浏览界面按 `o`） | 结果集按所选键重排，游标回到首页，物理栏位未移动 |
| 中途退出（入职/信息修改中输入 `q`） | 提示「已放弃当前操作，返回主菜单」，未写入任何数据 |
| 离职（逻辑删除） | 栏位总数不变，在岗减 1、空位加 1；按「人员状态=仅离职」可检索到该记录 |
| 入职（复用空位） | 提示「复用最靠前的空位」，栏位总数不变 |
| 区间统计 | 22 人，平均 46.0 岁，含性别/学历/职称/科室/领域分布 |
| 区间修改（批量晋升职称） | 共更新 12 条，工号同步重算，无冲突回滚；修改前后对照均用分页浏览 |
| 退出保存 / 重新加载 | `data.txt` 13 字段格式读写正常 |

---

## 七、面向业界真实身份信息管理系统的改进意见

> 说明：这里讨论的是**人事/人员主数据管理（HR Master Data / 人员信息管理）**，
> 即「谁是我们的人、他是什么职称、在哪个科室」这类**业务身份档案**。
> 它与 **IAM（身份与访问管理，解决"能不能登录、能访问哪些资源"）是两件事**，
> 本节不涉及账号、口令、SSO、权限令牌等 IAM 话题。
> 两者在真实系统里通常并存：本系统这类人事库是**权威数据源（Authoritative Source）**，
> IAM 从它同步人员状态来决定账号的启用/停用。

### 7.1 现状与差距

| 维度 | 当前实现 | 业界真实系统 |
| --- | --- | --- |
| 存储 | 内存顺序表 + 纯文本文件 | 关系数据库（含约束、索引、事务） |
| 并发 | 单机单用户 | 多用户并发读写 |
| 一致性 | 应用层校验 | 数据库约束 + 事务 |
| 历史 | 覆盖式修改 | 变更留痕、可追溯 |
| 安全 | 明文 | 加密存储、脱敏展示、分级授权 |
| 容量 | 百条级 | 十万~百万级，需要索引与分页 |

### 7.2 迁移到关系数据库：表结构设计

把顺序表映射为表，用**约束**替代应用层校验，用**索引**替代线性扫描：

```sql
-- 科室 / 领域 / 职称 / 学历 等字典表（对应程序里的枚举 + 中文名称表）
CREATE TABLE dict_dept (
    code   SMALLINT PRIMARY KEY,      -- 对应 Department 枚举值
    name   VARCHAR(32) NOT NULL UNIQUE
);

-- 人员主表
CREATE TABLE doctor (
    id            BIGSERIAL PRIMARY KEY,          -- 代理主键，与业务无关
    work_id       CHAR(8)      NOT NULL,          -- 8 位结构化工号（业务唯一键）
    id_card       CHAR(18)     NOT NULL,          -- 身份证号
    name          VARCHAR(32)  NOT NULL,
    gender        SMALLINT     NOT NULL CHECK (gender IN (0,1)),
    birth_date    DATE         NOT NULL,
    education     SMALLINT     NOT NULL,
    degree        SMALLINT     NOT NULL,
    major         VARCHAR(64)  NOT NULL,
    position      SMALLINT     NOT NULL,
    title         SMALLINT     NOT NULL,
    dept_code     SMALLINT     NOT NULL REFERENCES dict_dept(code),
    field_code    SMALLINT     NOT NULL,
    is_active     BOOLEAN      NOT NULL DEFAULT TRUE,   -- 对应 active 标识符
    hired_at      TIMESTAMP    NOT NULL DEFAULT now(),
    resigned_at   TIMESTAMP,                            -- 离职时间
    created_at    TIMESTAMP    NOT NULL DEFAULT now(),
    updated_at    TIMESTAMP    NOT NULL DEFAULT now()
);

-- 关键点：唯一约束只作用于「在岗」记录，离职记录不占用工号/身份证号
-- PostgreSQL / SQLite 支持部分唯一索引：
CREATE UNIQUE INDEX uq_doctor_workid_active
    ON doctor (work_id) WHERE is_active;
CREATE UNIQUE INDEX uq_doctor_idcard_active
    ON doctor (id_card) WHERE is_active;

-- 常用检索条件建索引
CREATE INDEX idx_doctor_name     ON doctor (name);
CREATE INDEX idx_doctor_birth    ON doctor (birth_date);
CREATE INDEX idx_doctor_dept     ON doctor (dept_code) WHERE is_active;
CREATE INDEX idx_doctor_title    ON doctor (title)     WHERE is_active;
```

**关于「逻辑删除」与「唯一约束」的冲突**（这是本程序 v2.0 需求 2 在数据库里的对应难点）：
如果直接对 `work_id` 建全局唯一索引，那么离职老员工占用的工号就无法被新人重用，
与需求 2 矛盾。解决办法有三类：

1. **部分唯一索引**（推荐，PostgreSQL / SQLite 支持）：
   只对 `is_active = TRUE` 的行做唯一性约束；
2. **MySQL 的替代做法**：MySQL 不支持部分索引，可把 `is_active` 并入唯一键，
   例如 `UNIQUE (work_id, active_flag)`，其中 `active_flag` 对在岗行为 `0`、
   离职行为该行的主键值（保证离职行彼此不冲突）；
3. **主表 + 历史表分离**：在岗人员放 `doctor`，离职后整行搬到 `doctor_history`，
   主表天然只有在岗记录，唯一约束直接生效，同时天然获得完整的任职历史。

### 7.3 如何在 C 程序里连接数据库

当前程序用 `fopen/fgets` 读写文本文件，替换为数据库只需把 `Service_Save` /
`Service_Load` 换成数据库访问函数，**其余各层不用改**（这正是分层的价值）。

**方案 A：SQLite（推荐用于课程作业升级，零部署）**

```bash
gcc ... -lsqlite3          # 需要 sqlite3.h / sqlite3.c
```

```c
#include <sqlite3.h>

static sqlite3* g_db = NULL;

int Db_Open(const char* path)
{
    return sqlite3_open(path, &g_db) == SQLITE_OK;
}

/* 插入：用占位符绑定参数，避免拼字符串造成的注入与转义问题 */
int Db_Hire(const Doctor* d)
{
    const char* sql =
        "INSERT INTO doctor (work_id,id_card,name,gender,birth_date,"
        "education,degree,major,position,title,dept_code,field_code,is_active) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,1)";
    sqlite3_stmt* st = NULL;
    char birth[16];
    int  rc;

    if (sqlite3_prepare_v2(g_db, sql, -1, &st, NULL) != SQLITE_OK) return 0;

    Date_ToStr(&d->birth, birth, sizeof birth);
    sqlite3_bind_text (st,  1, d->workId, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (st,  2, d->idCard, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (st,  3, d->name,   -1, SQLITE_TRANSIENT);
    sqlite3_bind_int  (st,  4, (int)d->gender);
    sqlite3_bind_text (st,  5, birth,     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int  (st,  6, (int)d->education);
    sqlite3_bind_int  (st,  7, (int)d->degree);
    sqlite3_bind_text (st,  8, d->major,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int  (st,  9, (int)d->position);
    sqlite3_bind_int  (st, 10, (int)d->title);
    sqlite3_bind_int  (st, 11, (int)d->dept);
    sqlite3_bind_int  (st, 12, (int)d->field);

    rc = (sqlite3_step(st) == SQLITE_DONE);   /* 唯一约束冲突会在这里报 SQLITE_CONSTRAINT */
    sqlite3_finalize(st);
    return rc;
}

/* 离职：等价于本程序的「切换不外显标识符」 */
/* UPDATE doctor SET is_active = 0, resigned_at = now() WHERE work_id = ? AND is_active = 1; */
```

**方案 B：MySQL / PostgreSQL（多用户、真实部署）**

- MySQL：使用 MySQL Connector/C（`mysql.h`，链接 `-lmysqlclient`），
  `mysql_real_connect` 建立连接，`mysql_stmt_*` 系列做参数化查询；
- PostgreSQL：使用 libpq（`libpq-fe.h`，链接 `-lpq`），
  `PQconnectdb` 建立连接，`PQexecParams` 做参数化查询。

```c
/* libpq 示例：按复合条件检索（对应本程序的 Query 结构体） */
const char* sql =
    "SELECT work_id,id_card,name,gender,birth_date,education,degree,"
    "       major,position,title,dept_code,field_code "
    "FROM doctor WHERE is_active AND ($1::int IS NULL OR dept_code = $1::int) "
    "  AND ($2::int IS NULL OR title = $2::int)";
const char* params[2] = { deptStr, titleStr };
PGresult* res = PQexecParams(conn, sql, 2, NULL, params, NULL, NULL, 0);
```

**工程要点：**

1. **参数化查询**：永远用占位符绑定，不要拼接 SQL 字符串；
2. **连接池**：多线程/多用户场景不要每次请求都建连接，使用连接池（如
   `libzdb`、或 MySQL 的 `mysql_pool`），避免连接建立开销与连接数耗尽；
3. **事务**：批量修改（如本程序的「区间修改：批量晋升职称」）必须放在一个事务里，
   要么全部成功、要么全部回滚：

   ```sql
   BEGIN;
   UPDATE doctor SET title = title + 1 WHERE ... ;
   -- 若某条重算出的 work_id 与在岗记录冲突，唯一索引会报错 → ROLLBACK
   COMMIT;
   ```
4. **配置外置**：连接串、账号口令不要硬编码在源码里，放到配置文件或环境变量，
   口令使用密钥管理服务保存；
5. **错误映射**：把数据库错误码（唯一约束冲突 `23505` 等）映射为业务语义，
   正好对应本程序的 `HIRE_FAIL_DUP_WORKID` / `HIRE_FAIL_DUP_IDCARD`。

### 7.4 其他值得改进的工程实践

1. **代理主键 + 业务键分离**：用自增 `id` 作主键，`work_id` 作业务唯一键。
   这样即使工号因职称晋升而重算，行与行之间的引用关系也不会断裂
   （本程序当前靠「栏位下标」隐含引用，一旦物理搬移就会失效）。
2. **工号派生规则数据库化**：把「科室+领域+职称+姓名 → 工号」写成数据库函数或
   应用层统一服务，并用唯一索引兜底，避免多入口写入时规则不一致。
3. **身份证号合规处理**：
   - 存储：身份证号属于个人敏感信息，落库前应**加密**（如 AES-GCM，密钥由 KMS 托管），
     或至少做列级加密；
   - 展示：默认**脱敏**（如 `110***********1234`），只有授权角色可见全量——
     这正是本程序「简略模式不显示身份证号」思路在真实系统里的延伸；
   - 校验：校验位、性别、出生日期的派生规则应集中在一处实现，避免各端重复。
4. **权限分级（RBAC）**：按角色控制字段级可见性（人事专员可看全量、科室主任只看本科室、
   普通员工只看自己），并记录**谁在什么时候看了谁**的访问日志。
5. **审计与版本**：对 `doctor` 表的每次变更写审计表
   （`who / when / before / after`），支持追溯与回滚；
   若需要完整历史，可采用**时态表**（`valid_from` / `valid_to`）或
   `doctor_history` 表。
6. **并发控制**：多用户同时修改同一条记录时，用乐观锁（`version` 列，
   `UPDATE ... WHERE id = ? AND version = ?`）或悲观锁（`SELECT ... FOR UPDATE`）。
7. **软删除之外的完整性**：离职时通常还要级联处理（停用账号、移交工作、结算），
   应把这些动作纳入**工作流/事务**，而不是只改一个标志位。
8. **性能**：记录数上到十万级后，姓名模糊查询（`LIKE '%xx%'`）无法走普通索引，
   可考虑**全文索引**或独立检索服务；分页使用 `LIMIT/OFFSET`（深分页改用游标）。
9. **备份与恢复**：定期全量备份 + 增量日志（binlog / WAL），并演练恢复流程。
10. **接口化**：把业务层（`service.c`）包装成 REST/gRPC 服务，
    控制台程序改为客户端，这样多终端、多系统才能共用同一份权威数据。
11. **数据质量**：入库时用约束与触发器保证「工号与属性一致」「在职记录身份证号唯一」
    「离职必须有离职时间」等不变式，而不是依赖调用方自觉。

---

## 八、已知限制

- 单机、单用户、无并发控制；数据量适合千条级以内；
- 数据文件为全量覆盖写，写入过程中断电可能损坏文件（真实系统应使用事务或
  先写临时文件再原子替换）；
- 身份证号、姓名等敏感信息明文存储，仅通过「简略模式」做展示层规避；
- 排序仅作用于结果集显示次序，不改变物理存储（这是为保持空位复用语义而做的取舍）；
- 工号派生依赖姓名哈希，理论上存在不同姓名派生出相同工号的可能，
  程序在入职时会检测冲突并拒绝，需人工调整（真实系统应在姓氏表/名码表上做更细的分配）。
