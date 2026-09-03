# dynamic_memory scaffold

malloc/calloc/realloc/free + 简易 vector 扩容 + dangling 防御。

## 复现

```bash
cd learning/scaffold/dynamic_memory
gcc -Wall -Wextra -O2 -std=c11 -o mem_demo main.c
./mem_demo
```

## 预期输出

```
[malloc] a = 0 1 4 9 16
[calloc] b = 0 0 0 0
[realloc] s = 'hello, world!'
[vec]    capacity grown, len=10 cap=16, items: 0 10 20 ...
[null]    p == NULL after free
```

## 关键点

- **xmalloc/xrealloc 包装** 是开源项目标准做法——遇到 OOM 立即 exit，避免每处都判 NULL
- **calloc 初始化为 0**——malloc 必须 memset 才为零，避免读到未初始化堆数据
- **realloc 返回值直接赋值给原指针是 bug**：失败时原指针泄漏。必须 `q = realloc(p, n)` 后判 `q`
- **2 倍扩容**：均摊 O(1) 插入；1.5 倍扩容空间更优但常数略差
- **dangling 指针**：free 后立即置 NULL，防止 use-after-free

详见 NOTES.md 工程对照段。
