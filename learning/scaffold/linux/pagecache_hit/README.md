# pagecache_hit — 页缓存命中率分析

## 简介

演示 Linux 页缓存机制，分析 `/proc/vmstat` 统计，理解缓存命中率计算。

## 编译

```bash
gcc -std=c11 -Wall -o pagecache_hit main.c
```

## 运行

```bash
./pagecache_hit
```

## 核心概念

### 页缓存 (Page Cache)

Linux 将磁盘文件缓存在内存页中，减少磁盘 I/O。`/proc/meminfo` 中的 `Cached` 即为页缓存大小。

### 命中率计算

```
命中率 = (1 - 主缺页错误 / 总缺页错误) × 100%
```

- **次缺页错误**：页已在内存，仅需建立页表映射
- **主缺页错误**：需要从磁盘加载数据

## 关键文件

- `/proc/vmstat` — 虚拟内存统计
- `/proc/meminfo` — 内存使用详情
- `/proc/buddyinfo` — 伙伴分配器状态

## 优化建议

- 顺序访问优于随机访问
- 热点数据应保持常驻内存
- 大文件顺序扫描利用预读机制
