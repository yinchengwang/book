---
name: clang_code_style
description: 基于 clang-format 的项目 C/C++ 代码风格 —— 从源码中提取，映射到 clang-format 配置
version: 1.0.0
source: source-code-analysis
analyzed_files: 50+
repo: C--code-book
trigger: 编辑 .c/.h/.cpp 文件或修改 .clang-format 时激活
---

# Clang 代码风格规范

基于项目全部 C/C++ 源文件采样分析，提取实际编码风格并映射为 clang-format 配置项。

## 1. 缩进与对齐

### 1.1 基本缩进

```c
// ✅ 项目风格：4 空格缩进，无 Tab
void bubble_sort(int *arr, int arr_size, int *res)
{
    memcpy(res, arr, sizeof(int) * arr_size);
    for (int i = 0; i < arr_size; i++) {
        for (int j = 0; j < arr_size - 1 - i; j++) {
            if (res[j] > res[j + 1]) {
                swap(&res[j], &res[j + 1]);
            }
        }
    }
}
```

**clang-format:**
```yaml
IndentWidth: 4
UseTab: Never
TabWidth: 4
```

### 1.2 续行缩进

```c
// ✅ 续行使用 4 空格对齐
static int32_t hnsw_quantization_params_are_valid(const faiss_hnsw_t *index,
                                                  const faiss_hnsw_quantization_params_t *params)
```

**clang-format:**
```yaml
ContinuationIndentWidth: 4
AlignAfterOpenBracket: Align
```

### 1.3 访问控制符（C++ 测试文件）

```cpp
// ✅ 项目风格：访问控制符与 class 同级
class ListUnitsTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};
```

**clang-format:**
```yaml
AccessModifierOffset: -4
```

## 2. 大括号风格

### 2.1 函数大括号

```c
// ✅ 主流风格：函数体 { 另起一行（Allman 风格）
int is_empty(max_heap_t *heap)
{
    return heap->size == 0;
}

// ⚠️ 也存在：{ 在同一行（K&R 风格）
int is_heap_full(max_heap_t *heap) {
    return heap->size == heap->capacity;
}
```

**规则：** 项目同时存在两种风格，推荐统一为 Allman（`{` 另起一行），与多数文件一致。

**clang-format:**
```yaml
BreakBeforeBraces: Allman
```

### 2.2 控制流大括号

```c
// ✅ if/while/for 的 { 在同一行
if (!node) {
    return NULL;
}
while (cur) {
    cur = cur->next;
}
```

**clang-format:**
```yaml
BraceWrapping:
  AfterControlStatement: Never
  AfterFunction: true
```

### 2.3 单语句块省略大括号

```c
// ✅ 项目中允许单语句省略大括号
if (!head)
    return head;

if (pHead1)
    head->next = pHead1;

// ⚠️ 但多行建议加上
if (!index || !params || index->quantization_type == QUANTIZATION_TYPE_NONE) {
    return 0;
}
```

**clang-format:**
```yaml
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
```

## 3. 空格规则

### 3.1 指针与引用

```c
// ✅ 项目风格：* 靠近变量名，空格在类型后
list_node_t *head = NULL;
void swap(int *lhs, int *rhs);
char *time_unit(char *time_str);

// ✅ 双重指针
void list_insert_head(list_node_t **head, int val);
```

**clang-format:**
```yaml
PointerAlignment: Right
```

### 3.2 括号与运算符

```c
// ✅ 关键字后加空格
if (a > b)
while (cur->next)
for (int i = 0; i < n; i++)
return val;

// ✅ 二元运算符两侧空格
int sum = n1 + n2 + carry;
i = i >> 1;

// ✅ 一元运算符紧贴操作数
i++;
(*count)++;

// ✅ 函数调用括号紧贴函数名
list_destroy(head);
memcpy(res, arr, sizeof(int) * arr_size);
```

**clang-format:**
```yaml
SpaceBeforeParens: ControlStatements
SpacesInParentheses: false
SpacesInCStyleCastParentheses: false
SpaceAfterCStyleCast: false
```

### 3.3 逗号与分号

```c
// ✅ 逗号后加空格，分号后加空格
int min_heap_del(min_heap_t *heap, int idx);
faiss_heap_push(i + 1, bh_val, bh_ids, x[i], ids[i], cmp);
for (int i = 0; i < arr_size; i++)
```

**clang-format:**
```yaml
SpaceAfterComma: true
SpaceBeforeComma: false
SpaceBeforeSemicolon: false
```

## 4. 换行规则

### 4.1 最大行宽

项目实际行宽在 80-120 字符之间波动。`faiss_heap.c` 中有些函数签名超过 100 字符。

```c
// 示例：长函数声明
inline void faiss_heap_push(uint32_t k, float *bh_val, int32_t *bh_ids,
                            float val, int32_t id, FaissHeapMaxCmpFunc cmp)
{
```

**clang-format:**
```yaml
ColumnLimit: 120
```

### 4.2 函数参数换行

```c
// ✅ 参数多时每个参数一行
static int32_t hnsw_quantization_params_are_valid(const faiss_hnsw_t *index,
                                                  const faiss_hnsw_quantization_params_t *params)

// ✅ 参数少时保持单行
void heap_item_swap(max_heap_t *heap, int idx1, int idx2)
```

**clang-format:**
```yaml
AllowAllParametersOfDeclarationOnNextLine: false
BinPackParameters: false
```

### 4.3 空行

```c
// ✅ 函数定义间保留一个空行
void swap(int *lhs, int *rhs)
{
    // ...
}

// 冒泡排序
void bubble_sort(int *arr, int arr_size, int *res)
{
    // ...
}
```

**clang-format:**
```yaml
SeparateDefinitionBlocks: Always
MaxEmptyLinesToKeep: 1
```

## 5. 注释风格

### 5.1 注释格式

```c
// ✅ 单行注释（项目主要风格）
// 冒泡排序：从序列头开始，每次选择相邻的元素进行比较

// ✅ 多行注释（少量使用）
/* Faiss's API: a generic heap implementation */

// ✅ TODO 标记
// TODO 希尔排序: 没看懂，需要理解算法实现细节
```

**clang-format:**
```yaml
CommentPragmas: '^ TODO'
SpacesBeforeTrailingComments: 2
```

### 5.2 注释语言

所有注释使用**中文**（项目 AGENTS.md 约定）。

## 6. Include 排序

```c
// ✅ 项目风格：系统头文件 → 空行 → 项目头文件
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "self_made/list.h"
#include "self_made/data_type.h"
```

**clang-format:**
```yaml
IncludeBlocks: Preserve
IncludeCategories:
  - Regex: '^<.*\.h>'
    Priority: 1
  - Regex: '^".*"'
    Priority: 2
SortIncludes: true
```

## 7. 类型与命名映射

这些是 clang-format 无法强制但对 clang-tidy 有意义的约定：

| 元素 | 项目风格 | clang-tidy 检查 |
|------|---------|----------------|
| 类型定义 | `typedef struct/union { } xxx_t;` | `readability-identifier-naming` |
| 函数名 | 蛇形命名 `list_node_create` | `readability-identifier-naming` |
| 常量 | 全大写 `MAX_SIZE` | `readability-identifier-naming` |
| 枚举值 | 全大写 `QUANTIZATION_TYPE_NONE` | `readability-identifier-naming` |
| 变量名 | 蛇形命名 `node_count` | `readability-identifier-naming` |
| 结构体成员 | 蛇形命名 `next`, `val` | `readability-identifier-naming` |

## 8. 推荐 .clang-format 配置

基于以上分析，推荐以下 `.clang-format` 配置：

```yaml
---
# 基础
BasedOnStyle: LLVM
Language: Cpp
Standard: c++17

# 缩进
IndentWidth: 4
TabWidth: 4
UseTab: Never
ContinuationIndentWidth: 4
AccessModifierOffset: -4
IndentCaseLabels: true

# 大括号
BreakBeforeBraces: Allman
BraceWrapping:
  AfterControlStatement: Never
  AfterFunction: true
  AfterStruct: false
  AfterEnum: false
  AfterUnion: false

# 空格
PointerAlignment: Right
SpaceBeforeParens: ControlStatements
SpaceAfterComma: true
SpaceBeforeComma: false
SpaceBeforeSemicolon: false
SpacesInParentheses: false
SpacesInCStyleCastParentheses: false
SpacesInSquareBrackets: false
SpaceAfterCStyleCast: false

# 换行
ColumnLimit: 120
MaxEmptyLinesToKeep: 1
SeparateDefinitionBlocks: Always
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
AllowShortFunctionsOnASingleLine: None
BinPackParameters: false
AllowAllParametersOfDeclarationOnNextLine: false

# Include
IncludeBlocks: Preserve
SortIncludes: true
IncludeCategories:
  - Regex: '^<.*\.h>'
    Priority: 1
  - Regex: '^".*"'
    Priority: 2

# 注释
SpacesBeforeTrailingComments: 2
ReflowComments: false

# 对齐
AlignConsecutiveAssignments: false
AlignConsecutiveDeclarations: false
AlignAfterOpenBracket: Align

# 其他
IndentPPDirectives: None
FixNamespaceComments: true
```

## 9. clang-tidy 推荐检查项

对项目最有价值的 clang-tidy 检查（按优先级）：

```bash
# 必开（内存安全）
clang-tidy --checks='-*,bugprone-*,clang-analyzer-*,cppcoreguidelines-pro-type-*,cppcoreguidelines-owning-memory'

# 推荐开（代码质量）
clang-tidy --checks='-*,readability-braces-around-statements,readability-identifier-naming,readability-implicit-bool-conversion,misc-*'
```

## 10. 代码风格检查清单

新增 C/C++ 文件时确认以下项：

- [ ] 使用 4 空格缩进（无 Tab）
- [ ] 函数大括号另起一行
- [ ] 指针 `*` 紧贴变量名
- [ ] 关键字后空格：`if (`, `while (`, `for (`
- [ ] 行宽不超过 120 字符
- [ ] Include 分组：系统头 → 空行 → 项目头
- [ ] 注释使用中文
- [ ] 函数间保留一个空行
- [ ] 蛇形命名：`模块_动作_对象`
- [ ] 模块内部函数声明为 `static`
