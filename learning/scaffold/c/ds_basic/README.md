# ds_basic scaffold

链表四件核心：单链表 CRUD、双向链表、哨兵节点、Floyd 判圈。

## 复现命令

```bash
cd learning/scaffold/ds_basic
gcc -Wall -Wextra -O2 -std=c11 -o dsbasic_demo main.c
./dsbasic_demo
```

## 预期输出

```
=== 单链表 CRUD ===
[slist] 10 -> 20 -> 30 -> NULL
[slist] find(20) = yes
[slist] delete(20)
[slist] 10 -> 30 -> NULL

=== 双向链表 ===
[dlist] 1 <-> 2 <-> 3 <-> NULL
[dlist-rev] 3 <-> 2 <-> 1 <-> NULL

=== 哨兵节点 ===
[sentinel] 遍历: 100 200

=== 循环检测 (Floyd) ===
[cycle] has_cycle(带环链表) = yes
[cycle] 带环链表未 free（避免 double-free）

=== PASS ===
```

## 关键点

- 单链表插入 O(1) 头插，删除需前驱指针
- 双向链表 prev/next 双链，可实现 O(1) 给定节点删除
- 哨兵节点消除 head==NULL 的特殊判断
- Floyd 判圈：快慢指针，相遇即有环

详见 NOTES.md 工程对照段。
