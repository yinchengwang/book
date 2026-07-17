# JIT 编译技术调研报告

**版本**：v1.0
**日期**：2026-07-15
**状态**：调研中

---

## 1. 概述

### 1.1 目标

评估 LLVM JIT 编译对 SQL 表达式求值的加速效果，为执行引擎 JIT 化提供技术储备。

### 1.2 背景

PostgreSQL 11+ 引入 LLVM JIT，对表达式求值、元组解构等场景实现了 20%-500% 的性能提升。

---

## 2. PostgreSQL JIT 架构

### 2.1 核心组件

```
┌─────────────────────────────────────────────────────────────────┐
│                     PostgreSQL JIT 架构                          │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     │
│  │  ExprState   │    │  LLVM IR     │    │  Machine Code│     │
│  │  (表达式树)   │ → │  (中间表示)   │ → │  (机器码)     │     │
│  └──────────────┘    └──────────────┘    └──────────────┘     │
│         ↑                  ↑                    ↑              │
│    JitCompileExpr    LLVM OrcJIT         ExecJitExpr          │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 关键 API

```c
/* PostgreSQL JIT 接口 */
typedef struct JitContext {
    LLVMContextRef   llvm_context;
    LLVMModuleRef     module;
    LLVMOrcModuleHandle handle;
} JitContext;

/* JIT 编译入口 */
JitExprState *ExecJitCompileExpr(ExprState *state);

/* JIT 执行 */
Datum ExecJitExpr(JitExprState *state, ExprContext *econtext);
```

---

## 3. LLVM JIT 关键技术

### 3.1 LLVM ORC JIT

- **ORC (On Request Compilation)**：按需编译
- **Lazy Compilation**：函数首次调用时编译
- **Symbol Resolution**：符号解析与链接

### 3.2 表达式编译流程

```
1. Expr 树遍历
   ↓
2. 生成 LLVM IR
   %res = add i32 %a, %b
   %cmp = icmp sgt i32 %res, 0
   br i1 %cmp, label %true, label %false
   ↓
3. 优化（O2/O3）
   ↓
4. 生成机器码
   mov eax, [rdi]
   add eax, [rsi]
   cmp eax, 0
   jg .true
   ↓
5. 执行
   call rax
```

### 3.3 类型映射

| PostgreSQL 类型 | LLVM IR 类型 |
|----------------|--------------|
| int32          | i32          |
| int64          | i64          |
| float          | float        |
| double         | double       |
| bool           | i1           |
| Datum          | i64          |

---

## 4. 性能收益分析

### 4.1 适合 JIT 的场景

| 场景 | 预期加速比 | 原因 |
|------|-----------|------|
| 复杂表达式 | 2-5x | 减少函数调用开销 |
| 简单表达式 | 0.8-1.2x | JIT 编译开销 |
| 大数据集扫描 | 1.5-3x | 分摊编译成本 |
| WHERE 过滤 | 2-4x | 分支预测优化 |
| 聚合计算 | 3-10x | 循环展开 |

### 4.2 PostgreSQL 实测数据

```sql
-- TPCH Q1 (聚合密集)
SELECT l_returnflag, l_linestatus,
       sum(l_quantity) as sum_qty,
       sum(l_extendedprice) as sum_base_price
FROM lineitem
WHERE l_shipdate <= date '1998-12-01' - interval '90' day
GROUP BY l_returnflag, l_linestatus;

-- 结果：JIT 开启后性能提升 5-20%
```

---

## 5. 实现方案

### 5.1 渐进式集成

**Phase 1：占位实现（当前）**
- 头文件定义接口
- 空实现降级为解释器
- 性能对比框架

**Phase 2：简单表达式 JIT**
- 常量 + 算术运算
- 无分支表达式

**Phase 3：完整表达式 JIT**
- 分支表达式
- 函数调用

**Phase 4：批处理优化**
- 向量化执行
- 循环展开

### 5.2 降级策略

```c
Datum ExecEvalExpr(ExprState *state, ExprContext *econtext) {
    if (state->jit_compiled && jit_available()) {
        return ExecJitExpr(state, econtext);
    } else {
        return ExecInterpExpr(state, econtext);
    }
}
```

---

## 6. 技术风险

### 6.1 编译延迟

- **问题**：首次执行需要编译，增加延迟
- **缓解**：预编译热点表达式

### 6.2 内存占用

- **问题**：JIT 代码占用内存
- **缓解**：限制编译缓存大小

### 6.3 平台依赖

- **问题**：LLVM 对不同平台支持不一致
- **缓解**：降级为解释器

---

## 7. 建议决策

### 7.1 短期（Phase 5）

- 实现占位接口，不引入 LLVM 依赖
- 建立性能对比框架
- 完成 1 个简单表达式的 JIT 原型

### 7.2 中期（Phase 6）

- 引入 LLVM 可选依赖
- 实现简单表达式 JIT
- 集成到执行引擎

### 7.3 长期（Phase 7+）

- 完整表达式 JIT
- 批处理优化
- GPU JIT 探索

---

## 8. 参考资料

- PostgreSQL 16 源码：`src/backend/jit/llvm/`
- LLVM ORC JIT：https://llvm.org/docs/ORCv2.html
- PostgreSQL JIT 文档：https://www.postgresql.org/docs/current/jit.html

---

*报告版本：v1.0*
*创建日期：2026-07-15*
