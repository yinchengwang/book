# 数据结构学习卡片：函数指针 (Function Pointer)

## 学习目标

1. 理解函数指针的概念：函数也有地址，可以像变量一样传递
2. 掌握 typedef 函数指针的声明方式，使代码更易读
3. 理解回调函数模式：函数作为参数传递给其他函数
4. 掌握策略模式：运行时选择不同的算法实现
5. 掌握 qsort/bsearch 比较器的实现方式

## 核心概念

### 函数指针基础

```c
// 声明一个指向返回 int、参数为 (const void*, const void*) 的函数的指针
int (*compare)(const void *, const void *);

// 赋值：函数名就是函数的地址
compare = cmp_int_asc;

// 调用
compare(&a, &b);
```

### typedef 函数指针

```c
// 使用 typedef 给函数指针类型起别名
typedef int (*CompareFunc)(const void *, const void *);

// 之后声明变量更简洁
CompareFunc cmp = cmp_int_asc;
```

### 回调函数模式

```c
// 通用数组转换函数，接收转换函数作为参数
void array_transform(int *arr, size_t n, TransformFunc transform);

// 使用
array_transform(data, n, abs_value);  // 取绝对值
array_transform(data, n, negate);     // 取反
```

### 策略模式

```c
typedef struct {
    const char *name;
    int (*execute)(int);  // 策略函数
} Strategy;

// 运行时选择策略
Strategy *selected = user_choice ? &strategy_a : &strategy_b;
result = selected->execute(value);
```

### qsort 比较器约定

```c
// qsort 的比较函数必须返回：
//   < 0: a < b（a 应该排在 b 前面）
//   = 0: a == b
//   > 0: a > b（a 应该排在 b 后面）

int cmp_int_asc(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return ia - ib;  // 升序
}

int cmp_int_desc(const void *a, const void *b) {
    return -cmp_int_asc(a, b);  // 降序：取反
}
```

## 时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 函数指针调用 | O(1) | 间接调用，无额外开销 |
| qsort 排序 | O(n log n) | 标准快速排序 |
| bsearch 查找 | O(log n) | 二分查找 |
| 回调过滤 | O(n) | 线性扫描 |

## 编译与运行

```bash
# 编译
make

# 运行
make check

# 清理
make clean
```

## 目录结构

```
scaffold/ds/function_pointer/
├── main.c     # 主程序，演示函数指针与回调
├── Makefile   # 构建文件
├── README.md  # 学习目标与使用说明
└── NOTES.md   # 工程对照笔记
```
