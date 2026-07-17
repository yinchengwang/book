# string_match scaffold

四种经典字符串匹配算法：Brute Force（朴素匹配）、KMP（Knuth-Morris-Pratt）、Boyer-Moore 简化版（坏字符规则）、Rabin-Karp（滚动哈希）。

## 复现命令

```bash
cd learning/scaffold/string_match
gcc -Wall -Wextra -O2 -std=c11 -o stringmatch_demo main.c
./stringmatch_demo
```

## 预期输出

### KMP lps 数组
```
[kmp lps] building lps array for "ABABCABAB":
[kmp lps] lps[0] = 0
[kmp lps] lps[1] = 0 (mismatch at len=0)
[kmp lps] lps[2] = 1 (pattern[2]='A' == pattern[0]='A')
...
[kmp lps] final lps table: [0 0 1 2 0 1 2 3 4]
```

### Boyer-Moore 跳转
```
[bm bc] bad-char table (non -1 entries):
[bm bc]   'A' (ASCII 65) → last_seen=4
[bm bc]   'B' (ASCII 66) → last_seen=8
[bm] mismatch at s=0 j=4 ('D'!='C'), shift=4
```

### Rabin-Karp 滚动哈希
```
[rk] pattern hash = 37, window[0..8] hash = 12
[rk window 0] hash(text[i..i+8])=12 != phash=37, skip
[rk window 1] hash(text[i..i+8])=21 != phash=37, skip
...
[rk] ✓ found at index 10 (hash=37)
```

## 关键点

- **KMP 的核心是 lps 数组**：记录 pattern 中每个位置的最长相同前后缀长度，失配时跳过已匹配部分
- **BM 从右往左比较**：利用坏字符规则跳过尽可能多的位置，在长文本中平均 O(n/m) 次比较
- **RK 用 hash 加速**：滚动哈希 O(1) 计算下一窗口 hash，hash 碰撞需逐字符确认
- **四种算法各有适用场景**：短 pattern 用 BF 够快，重复前缀多用 KMP，大字母表用 BM，多模式匹配用 RK

详见 NOTES.md 工程对照段。
