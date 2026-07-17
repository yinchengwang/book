# 最小生成树 (Minimum Spanning Tree)

## 测试图

```
     1     4
v0 ─── v1 ─── v2
│╲     │     ╱│
3│  ╲5 │   2/ │3
│    ╲│╱    │
v4 ─── v3 ─── v5
     2     6
```

## 预期 MST

Prim/Kruskal 结果：v0-v1(1), v1-v3(2), v2-v3(2), v0-v4(3), v2-v5(3)
总权重：11

## 运行

```bash
gcc -std=c11 -Wall -Wextra -O2 -o mst_demo main.c && ./mst_demo
```
