# NOTES.md - 逻辑计划工程对照

## 逻辑计划概念

### 关系代数操作

1. **Scan**: 表扫描 (TableScan, IndexScan)
2. **Project**: 投影 (π)
3. **Filter**: 选择 (σ)
4. **Join**: 连接 (⋈)
5. **Aggregate**: 聚合 (γ)
6. **Sort**: 排序
7. **Limit**: 限制

### 代价模型

```
总代价 = Σ(行数 × 每行代价)

- SeqScan: rows × 1.0
- IndexScan: height × 1.2 + leaf × 0.5
- HashJoin: build + probe
```

### 规则优化

1.谓词下推
2. 列裁剪
3. 常量折叠
4. 连接重排

## 面试高频题

1. 逻辑计划和物理计划区别
2. 代价模型如何计算
3. CBO vs RBO