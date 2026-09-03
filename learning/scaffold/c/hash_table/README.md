# hash_table scaffold

哈希表双策略：djb2/murmur 哈希函数 + 链地址法 + 开放定址法（线性探测）+ 负载因子与 rehash。

## 复现命令

```bash
cd learning/scaffold/hash_table
gcc -Wall -Wextra -O2 -std=c11 -o hash_demo main.c
./hash_demo
```

## 预期输出

```
=== 哈希函数 ===
djb2('hello')     = 210714636441
murmur('hello')   = 2810334414
...

=== 链地址法 (Chaining) ===
[ch] put('apple',100)  load=0.12
...
[bucket 0] (空)
[bucket 1] 'fig':600 -> NULL
[bucket 2] 'elder':500 -> 'cherry':300 -> NULL
...
[ch] get('cherry') = yes
[ch] get('grape')  = no

=== 开放定址法 (线性探测) ===
[oh] put('dog',10) load=0.09
[oh] put('cat',20) load=0.18
...
[slot  0] 'pig':60
[slot  1] (空)
...
[oh] get('fish') = yes

=== Rehash 说明 ===
链地址法 load=0.75 (75%), 开放定址法 load=0.55 (55%)
实际系统中, 链地址法 load>1.0 才 rehash, 开放定址法 load>0.7 就 rehash

=== PASS ===
```

## 关键点

- djb2 简单快速，适合字符串；MurmurHash 分布更均匀但稍慢
- 链地址法负载因子可 >1.0；开放定址法必须 <1.0（表不能满）
- 线性探测简单但易产生"主聚类"（primary clustering）——连续占用的槽导致探测链越来越长
- rehash 触发阈值：链地址法 λ>1.0，开放定址法 λ>0.7

详见 NOTES.md 工程对照段。
