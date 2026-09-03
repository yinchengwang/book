# stdlib scaffold

C 标准库 `<stdlib.h>` 演示——malloc/calloc/realloc + exit/atexit + atoi/strtol + qsort/bsearch + rand/srand。

## 复现命令

```bash
cd learning/scaffold/stdlib

gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
```

## 预期输出（节选）

```
[malloc] === malloc/calloc/realloc/free ===
  calloc(5*int): 0 0 0 0 0 (全部 0)
  realloc 5→10: 0 1 2 3 4 5 6 7 8 9

[conv] === atoi / strtol ===
  atoi("12345") = 12345 (不检错)
  strtol("0x1F", base=16) = 31 (end=)
  strtol("999abc", base=10) = 999 (end=abc, 停在非数字)

[qsort] === qsort 排序 ===
  原序: 5 2 8 1 9 3
  升序: 1 2 3 5 8 9
  struct 按 id: {1,bob} {2,carol} {3,alice}

[bsearch] === bsearch 二分查找 ===
  bsearch(7) = 7 (在 sorted[3])
  bsearch(8) = NULL

[rand] === rand / srand ===
  5 个 [0, RAND_MAX] 随机数: 705419156 1916012467 ...

=== PASS ===
```

## 关键点

- **`malloc` 不初始化**：`calloc` 自动清零；`realloc` 可扩容/缩容
- **`atexit` LIFO 顺序**：后注册先调用，`return 0` 触发整条回调链
- **`atoi` 不检错**：转换失败返回 0，无法区分"输入 0"和"输入失败"。**用 `strtol` 更安全**
- **`strtol` 三参数**：`(str, &endptr, base)`，`endptr` 指向第一个非法字符
- **`qsort` 是通用排序**：通过 `void*` + 比较函数实现 int/double/struct 任意类型
- **`bsearch` 要求有序数组**：先 `qsort` 再 `bsearch`，比较函数与 qsort 一致
- **`rand` 不安全**：周期短、易预测。**生产用 `arc4random`/`/dev/urandom`**

详见 NOTES.md 工程对照段。