# 工程对照笔记

## 概述

本学习卡片演示的栈概念直接对应工程代码中的实际应用。以下对照工程源码中的栈实现与使用模式，帮助理解理论与实践的关联。

---

## 对照一：algo-prod 通用栈实现

**源文件**：`engineering/src/algo-prod/stack/stack.c`

工程中的通用栈库支持泛型，展示了专业的栈实现：

```c
#define STACK_DEFAULT_CAPACITY 16u
#define STACK_GROWTH_FACTOR 2u

struct stack {
    void *data;                  /* 泛型数据指针 */
    size_t size;                 /* 当前元素数 */
    size_t capacity;             /* 容量 */
    size_t element_size;         /* 元素大小（泛型需要） */
    void (*free_element)(void *); /* 元素析构回调 */
};

/* 入栈操作 */
int stack_push(stack_t *stack, const void *element)
{
    if (!stack || !element) return -1;
    /* 容量不足时自动扩容 */
    if (stack->size >= stack->capacity && stack_grow(stack) != 0) {
        return -1;
    }
    /* 按元素大小复制到栈中 */
    memcpy(stack_elem(stack, stack->size), element, stack->element_size);
    stack->size += 1u;
    return 0;
}

/* 出栈操作 */
int stack_pop(stack_t *stack, void *out)
{
    if (!stack || stack->size == 0u) return -1;

    stack->size -= 1u;
    unsigned char *top = stack_elem(stack, stack->size);
    if (out) {
        memcpy(out, top, stack->element_size);  /* 复制输出 */
    } else if (stack->free_element) {
        stack->free_element(top);  /* 释放内存 */
    }
    return 0;
}
```

**对照学习点**：
- 本卡片的 `ArrayStack` 与 `algo-prod/stack` 结构完全一致
- 2 倍扩容因子与本卡片一致
- `free_element` 回调用于释放堆上分配的元素
- 使用 `memcpy` + `element_size` 实现泛型，与本卡片 `int` 类型对照

---

## 对照二：函数调用栈帧

**源文件**：编译器自动生成，无源码文件

函数调用栈是栈最直接的应用，理解栈帧结构至关重要：

```
高地址
+------------------+  <-- 被调用函数栈帧
|    返回地址       |  (函数结束后返回的位置)
+------------------+
|    保存的 ebp     |  (调用者的栈帧基址)
+------------------+
|    参数 3         |  (从右往左压栈)
+------------------+
|    参数 2         |
+------------------+
|    参数 1         |
+------------------+  <-- 调用者栈帧
|    局部变量       |
+------------------+
低地址
```

**栈帧操作示例（C 代码模拟）**：

```c
/* 本卡片 demo_array_stack 模拟了栈帧操作 */
typedef struct {
    int *data;      /* 等价于：局部变量存储区 */
    size_t size;    /* 等价于：栈指针偏移 */
    size_t capacity;/* 等价于：栈大小限制 */
} ArrayStack;

/* astack_push 等价于：函数调用时的参数压栈
 * astack_pop  等价于：函数返回时的参数弹栈 + 返回地址跳转
 */
```

**对照学习点**：
- 函数调用的本质就是"压栈"：参数、返回地址、局部变量依次入栈
- `astack_push()` 模拟了函数调用时向栈帧添加数据
- `astack_pop()` 模拟了函数返回时弹出栈帧
- 递归调用过深会导致栈溢出（stack overflow）

---

## 对照三：dict 的 token 数组动态栈

**源文件**：`engineering/src/algo-prod/dict/dict_core.c`

```c
/* token 数组的动态扩展，模拟栈的 push 操作 */
int dict_append_token(token_t **tokens,
                     int32_t *token_count,
                     int32_t *token_capacity,
                     const char *text,
                     int32_t byte_start,
                     int32_t byte_length,
                     bool is_ascii)
{
    /* 容量不足时 2 倍扩容（栈的 resize） */
    if (*token_count >= *token_capacity) {
        new_capacity = *token_capacity > 0 ? *token_capacity * 2 : 8;
        new_tokens = (token_t *)realloc(*tokens,
            (size_t)new_capacity * sizeof(token_t));
        if (!new_tokens) return -1;
        *tokens = new_tokens;
        *token_capacity = new_capacity;
    }

    /* 入栈：复制 token 数据 */
    (*tokens)[*token_count].text = dict_strdup_slice(text, byte_start, byte_length);
    (*tokens)[*token_count].byte_start = byte_start;
    (*tokens)[*token_count].byte_length = byte_length;
    (*tokens)[*token_count].is_ascii = is_ascii;
    *token_count += 1;  /* size++ */
    return 0;
}
```

**对照学习点**：
- `token_count` 相当于本卡片的 `size`（栈顶指针）
- `token_capacity` 相当于本卡片的 `capacity`
- 2 倍扩容策略与本卡片完全一致
- `realloc` 实现动态扩展，与本卡片 `astack_resize` 一致

---

## 对照四：括号匹配与表达式求值

**源文件**：编译器/解释器中常见，无独立源码

本卡片的 `infix_to_postfix` 函数实现了经典算法：

```
中缀表达式: (2 + 3) * 4
后缀表达式: 2 3 + 4 *

求值过程:
1. Push 2      -> stack: [2]
2. Push 3      -> stack: [2, 3]
3. 遇到 '+': Pop 3, Pop 2, Push 5 -> stack: [5]
4. Push 4      -> stack: [5, 4]
5. 遇到 '*': Pop 4, Pop 5, Push 20 -> stack: [20]
结果: 20
```

**对照学习点**：
- 编译器后端常用后缀表达式进行代码生成
- 虚拟机的字节码指令也是后缀形式
- 括号匹配的栈算法与本卡片完全一致

---

## 工程代码模式总结

| 组件 | 栈类型 | 特点 | 用途 |
|------|--------|------|------|
| algo-prod/stack | 泛型顺序栈 | 2x扩容 + memcpy | 通用栈容器 |
| 函数调用 | 系统栈 | 编译器自动管理 | 函数调用/返回 |
| dict token | 动态数组 | 2x扩容 | 分词结果存储 |
| 表达式求值 | 算法栈 | 运算符栈 | 编译前端 |

---

## 延伸阅读

- `engineering/src/algo-prod/stack/stack.c` - 通用栈库实现
- `engineering/src/algo-prod/dict/dict_core.c` - token 数组的栈式扩展
