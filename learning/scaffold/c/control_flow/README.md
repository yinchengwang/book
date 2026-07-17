# control_flow scaffold

C 语言控制流演示——if/switch/for/while/do-while/goto + break/continue。

## 复现命令

```bash
cd learning/scaffold/control_flow

gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
```

## 预期输出（节选）

```
[if] === if/else 早 return 减少嵌套 ===
  classify(-5) = -1
  classify(15) = 2

[switch] === switch 多 case + fallthrough ===
  op=0 -> case 0: zero
  op=1 -> case 1: one
  case 1/2: 合并分支
  op=3 -> case 3/4/5: 三合一

[goto] === goto 跳出嵌套循环 ===
  found 5 at [1][1]
  not found 99

[break-continue] === break/continue 行为 ===
  break: 0 1 2 3 4
  continue (skip 5): 0 1 2 3 4 6 7 8 9

=== PASS ===
```

## 关键点

- **早 return 减少嵌套**：`if (bad) return ERR;` 把错误处理压扁，主逻辑在最浅的缩进层
- **`switch` fallthrough**：C 不要求每个 case 都有 `break`，但**默认 fallthrough 是历史包袱**——可用 `[[fallthrough]]` (C23) 或注释明示
- **`goto` 的合法用法**：跳出多层嵌套循环 + 集中 cleanup 代码（Linux 内核重度使用）
- **break vs continue**：break 跳出最近循环/switch，continue 跳到本轮末尾

详见 NOTES.md 工程对照段。