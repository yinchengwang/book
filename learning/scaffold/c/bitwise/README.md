# bitwise scaffold

位运算五件套：位掩码 set/clear/toggle/test、移位乘除、Brian Kernighan popcount、bitset 集合、工程位字段模拟。

## 复现命令

```bash
cd learning/scaffold/bitwise
gcc -Wall -Wextra -O2 -std=c11 -o bitwise_demo main.c
./bitwise_demo
```

## 关键点

- **位掩码**：`|=1<<n` 置位、`&=~(1<<n)` 清零、`^=1<<n` 翻转、`&(1<<n)` 测试——四个原语覆盖所有位操作
- **左移=乘 2^k**：编译器在优化时会自动将 `x*8` 替换为 `x<<3`（对无符号数）
- **Brian Kernighan 算法**：`n &= (n-1)` 每次消除最低位的 1——popcount 的业界标准实现。复杂度 O(1 的个数) 而非 O(位宽)
- **bitset**：用 `uint32_t words[N]` 按位存集合——交集=`&`、并集=`|`、差集=`& ~`，全都是单周期 SIMD 指令

详见 NOTES.md 工程对照段。
