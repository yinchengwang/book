/**
 * @file main.c
 * @brief 执行引擎总结：端到端流程 + 架构图 + 关键概念
 */

#include <stdio.h>
#include <stdlib.h>

/* ========================================================================
 * SQL 执行引擎端到端流程
 * ======================================================================== */

/**
 * Query Execution Pipeline:
 *
 *   SQL Text
 *      ↓
 *   [Parser] → AST (抽象语法树)
 *      ↓
 *   [Rewriter] → Rewritten AST (谓词下推、常量折叠)
 *      ↓
 *   [Planner] → Logical Plan (关系代数)
 *      ↓
 *   [Optimizer] → Physical Plan (执行计划)
 *      ↓
 *   [Executor] → Result Tuples (火山模型)
 *      ↓
 *   Storage Engine
 */

void print_pipeline(void) {
    printf("=== SQL 执行引擎流程 ===\n\n");

    printf("1. Parser (解析)\n");
    printf("   SQL → AST\n");
    printf("   - 词法分析: SQL → Token 流\n");
    printf("   - 语法分析: Token → AST\n\n");

    printf("2. Rewriter (重写)\n");
    printf("   AST → Rewritten AST\n");
    printf("   - 谓词下推\n");
    printf("   - 子查询展开\n");
    printf("   - 常量折叠\n\n");

    printf("3. Planner (规划)\n");
    printf("   AST → Logical Plan\n");
    printf("   - 逻辑代数转换\n");
    printf("   - 规则优化\n\n");

    printf("4. Optimizer (优化)\n");
    printf("   Logical Plan → Physical Plan\n");
    printf("   - 代价估算\n");
    printf("   - Join 重排\n");
    printf("   - 索引选择\n\n");

    printf("5. Executor (执行)\n");
    printf("   Physical Plan → Tuples\n");
    printf("   - 火山模型\n");
    printf("   - 算子树遍历\n");
}

/* ========================================================================
 * 火山模型 (Volcano Model)
 * ======================================================================== */

/**
 * Volcano Iterator Model:
 *
 *      +--------+
 *      |  Top   |
 *      +---+----+
 *          |
 *    +-----+-----+
 *    |           |
 *  +-+--+      +-+--+
 *  |mid |      |mid |
 *  +----+      +----+
 *       \      /
 *        \    /
 *      +--+--+
 *      | Bot |
 *      +-----+
 */

void print_volcano(void) {
    printf("=== 火山模型 (Volcano Iterator Model) ===\n\n");

    printf("核心接口:\n");
    printf("  open()   - 初始化算子\n");
    printf("  next()   - 获取下一个元组\n");
    printf("  close()  - 清理资源\n\n");

    printf("执行流程:\n");
    printf("  1. 从根算子调用 next()\n");
    printf("  2. 根算子调用子算子的 next()\n");
    printf("  3. 递归直到叶子算子 (SeqScan)\n");
    printf("  4. 叶子算子返回元组\n");
    printf("  5. 逐级向上处理\n\n");

    printf("特点:\n");
    printf("  ✓ 简单统一接口\n");
    printf("  ✓ 延迟计算\n");
    printf("  ✗ 函数调用开销大\n");
    printf("  ✗ 难以向量化\n");
}

/* ========================================================================
 * 执行引擎架构
 * ======================================================================== */

void print_architecture(void) {
    printf("=== 执行引擎架构 ===\n\n");

    printf("┌─────────────────────────────────────────────────────────┐\n");
    printf("│                    SQL 执行引擎                          │\n");
    printf("├─────────────────────────────────────────────────────────┤\n");
    printf("│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐    │\n");
    printf("│  │ Parser  │→ │Rewriter │→ │Planner  │→ │Optimizer│    │\n");
    printf("│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘    │\n");
    printf("│       │            │            │            │          │\n");
    printf("│       └────────────┴─────┬──────┴────────────┘          │\n");
    printf("│                          ↓                              │\n");
    printf("│                   ┌─────────────┐                       │\n");
    printf("│                   │  Executor  │                       │\n");
    printf("│                   └──────┬──────┘                       │\n");
    printf("│       ┌─────────┬───────┼───────┬─────────┐            │\n");
    printf("│       ↓         ↓       ↓       ↓         ↓            │\n");
    printf("│   ┌──────┐ ┌────┐ ┌──────┐ ┌──────┐ ┌──────┐           │\n");
    printf("│   │Scan  │ │Join│ │Agg   │ │Sort  │ │Modify│           │\n");
    printf("│   └──┬───┘ └─┬──┘ └──┬───┘ └──┬───┘ └──┬───┘           │\n");
    printf("│      └────────┴───────┴───────┴────────┘              │\n");
    printf("│                          ↓                              │\n");
    printf("│                   ┌─────────────┐                       │\n");
    printf("│                   │  Storage    │                       │\n");
    printf("│                   │  (Buffer)   │                       │\n");
    printf("│                   └─────────────┘                       │\n");
    printf("└─────────────────────────────────────────────────────────┘\n");
}

/* ========================================================================
 * 执行引擎 vs 向量化引擎
 * ======================================================================== */

void print_exec_models(void) {
    printf("=== 执行模型对比 ===\n\n");

    printf("┌────────────────┬────────────────────┬────────────────────┐\n");
    printf("│     特性       │     火山模型        │     向量化执行      │\n");
    printf("├────────────────┼────────────────────┼────────────────────┤\n");
    printf("│  处理方式      │  行式 (一次一行)    │  列式 (一次一批)   │\n");
    printf("│  CPU 利用率    │  低 (分支预测差)   │  高 (SIMD)         │\n");
    printf("│  内存访问      │  随机访问          │  顺序访问          │\n");
    printf("│  代表系统      │  PostgreSQL        │  ClickHouse        │\n");
    printf("└────────────────┴────────────────────┴────────────────────┘\n\n");

    printf("适用场景:\n");
    printf("  火山模型: OLTP (事务处理), 小数据量\n");
    printf("  向量化:   OLAP (分析处理), 大数据量\n");
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv) {
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║        DB SQL 执行引擎总结                                ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    print_pipeline();
    printf("\n");

    print_volcano();
    printf("\n");

    print_architecture();
    printf("\n");

    print_exec_models();

    printf("\n=== 演示完成 ===\n");
    return 0;
}