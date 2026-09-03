# stack_queue scaffold

栈与队列四变体：数组栈、链式栈、循环队列、双端队列。

## 复现命令

```bash
cd learning/scaffold/stack_queue
gcc -Wall -Wextra -O2 -std=c11 -o stackq_demo main.c
./stackq_demo
```

## 预期输出

```
=== 数组栈 ===
[astack] top=0 | 10
[astack] top=1 | 10 20
[astack] top=2 | 10 20 30
[astack] pop = 30
[astack] top=1 | 10 20

=== 链式栈 ===
[lstack] 100 -> NULL
[lstack] 200 -> 100 -> NULL
[lstack] pop = 200
[lstack] 100 -> NULL

=== 循环队列 ===
[cqueue] size=1 head=0 tail=1 | 1
[cqueue] size=2 head=0 tail=2 | 1 2
[cqueue] size=3 head=0 tail=3 | 1 2 3
[cqueue] dequeue = 1
[cqueue] size=2 head=1 tail=3 | 2 3
[cqueue] size=5 head=1 tail=1 | 2 3 4 5 6

=== 双端队列 ===
[deque] size=1 | 5
[deque] size=2 | 3 5
[deque] size=3 | 3 5 7
[deque] size=4 | 1 3 5 7

=== PASS ===
```

## 关键点

- 数组栈 O(1) push/pop，容量固定；链式栈无容量限制但每次 push 需 malloc
- 循环队列用 `head/tail/size` 三元组避免"判空判满"的歧义
- 双端队列两端均可 O(1) 插入/删除，是滑动窗口最大值的理想数据结构
- 所有队列变体在 `tail` 绕回 `head` 时依赖模运算

详见 NOTES.md 工程对照段。
