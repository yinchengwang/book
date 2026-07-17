# checkpoint - Checkpoint 机制

## 类型

- **Sharp**：刷所有脏页，耗时但一致
- **Fuzzy**：只刷检查点开始前的脏页

## 编译运行

```bash
make && ./test
```

## 参考实现

参考 `engineering/src/db/storage/wal/wal.c` 中的检查点逻辑。
