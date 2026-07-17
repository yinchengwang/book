/**
 * @file examples/api_demo.c
 * @brief Build My DB API 使用示例
 *
 * 演示如何使用数据库内核的 C API 进行：
 * - KV 操作
 * - SQL 解析和执行
 * - 事务管理
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <db/kv.h>
#include <db/sql/sql.h>
#include <db/sql/sql_exec.h>
#include <db/txn.h>
#include <db/performance/performance.h>

/* ─────────────────────────────────────────────────────────────────
 * KV API 示例
 * ───────────────────────────────────────────────────────────────── */

void kv_api_demo(void)
{
    printf("=== KV API 示例 ===\n\n");

    /* 打开/创建数据库 */
    kv_t *db = kv_open("./kv_demo.db");
    if (!db) {
        printf("打开数据库失败\n");
        return;
    }

    /* 插入键值对 */
    const char *key = "user:1001";
    const char *value = "Alice Smith";
    int ret = kv_put(db, key, value, strlen(value));
    printf("插入: %s = %s (ret=%d)\n", key, value, ret);

    /* 读取键值对 */
    char buf[256];
    size_t len = kv_get(db, key, buf, sizeof(buf));
    if (len > 0) {
        buf[len] = '\0';
        printf("读取: %s = %s\n", key, buf);
    }

    /* 检查键是否存在 */
    bool exists = kv_exists(db, key);
    printf("存在检查: %s exists=%d\n", key, exists);

    /* 更新值 */
    value = "Alice Johnson";
    kv_put(db, key, value, strlen(value));
    len = kv_get(db, key, buf, sizeof(buf));
    buf[len] = '\0';
    printf("更新后: %s = %s\n", key, buf);

    /* 删除键 */
    kv_delete(db, key);
    exists = kv_exists(db, key);
    printf("删除后: %s exists=%d\n\n", key, exists);

    /* 扫描键 */
    kv_put(db, "key:1", "value1", 6);
    kv_put(db, "key:2", "value2", 6);
    kv_put(db, "key:3", "value3", 6);
    kv_put(db, "other:1", "other_value", 11);

    printf("扫描 'key:' 前缀:\n");
    kv_iter_t *iter = kv_scan(db, "key:");
    char scan_key[64], scan_value[64];
    while (kv_iter_next(iter, scan_key, scan_value)) {
        printf("  %s = %s\n", scan_key, scan_value);
    }
    kv_iter_free(iter);

    kv_close(db);
    printf("\n");
}

/* ─────────────────────────────────────────────────────────────────
 * SQL API 示例
 * ───────────────────────────────────────────────────────────────── */

void print_result(sql_result_t *result)
{
    if (!result) return;

    size_t num_cols = sql_result_num_columns(result);
    size_t num_rows = sql_result_num_rows(result);

    if (num_cols == 0) {
        printf("影响 %zu 行\n", num_rows);
        return;
    }

    /* 打印表头 */
    printf("|");
    for (size_t c = 0; c < num_cols; c++) {
        printf(" %-15s |", sql_result_column_name(result, c));
    }
    printf("\n");

    /* 打印分隔线 */
    printf("+");
    for (size_t c = 0; c < num_cols; c++) {
        printf("-----------------+");
    }
    printf("\n");

    /* 打印数据行 */
    for (size_t r = 0; r < num_rows; r++) {
        void *values;
        sql_result_get_row(result, r, &values);
        printf("|");
        for (size_t c = 0; c < num_cols; c++) {
            char *val = ((char **)values)[c];
            printf(" %-15s |", val ? val : "NULL");
        }
        printf("\n");
        free(values);
    }
    printf("(%zu 行)\n", num_rows);
}

void sql_api_demo(void)
{
    printf("=== SQL API 示例 ===\n\n");

    /* 打开数据库 */
    kv_t *db = kv_open("./sql_demo.db");
    if (!db) {
        printf("打开数据库失败\n");
        return;
    }

    /* 创建 SQL 执行器 */
    sql_exec_t *exec = sql_exec_create(db);
    if (!exec) {
        printf("创建执行器失败\n");
        kv_close(db);
        return;
    }

    /* 1. 创建表 */
    printf("--- 创建表 ---\n");
    sql_node_t *node = sql_parse_one("CREATE TABLE users (id INT, name VARCHAR(100))");
    if (node) {
        sql_exec_ddl(exec, node);
        sql_node_free(node);
        printf("表创建成功\n");
    }

    /* 2. 插入数据 */
    printf("\n--- 插入数据 ---\n");
    const char *inserts[] = {
        "INSERT INTO users VALUES (1, 'Alice')",
        "INSERT INTO users VALUES (2, 'Bob')",
        "INSERT INTO users VALUES (3, 'Charlie')",
        "INSERT INTO users VALUES (4, 'Diana')",
        "INSERT INTO users VALUES (5, 'Edward')"
    };

    for (int i = 0; i < 5; i++) {
        node = sql_parse_one(inserts[i]);
        if (node) {
            sql_result_t *result = sql_exec(exec, node);
            if (result) {
                print_result(result);
                sql_result_free(result);
            }
            sql_node_free(node);
        }
    }

    /* 3. 查询数据 */
    printf("\n--- 查询数据 ---\n");
    const char *queries[] = {
        "SELECT * FROM users",
        "SELECT * FROM users WHERE id > 2",
        "SELECT * FROM users WHERE id = 1 OR id = 3"
    };

    for (int i = 0; i < 3; i++) {
        printf("\n查询 %d: %s\n", i + 1, queries[i]);
        node = sql_parse_one(queries[i]);
        if (node) {
            sql_result_t *result = sql_exec(exec, node);
            if (result) {
                print_result(result);
                sql_result_free(result);
            } else {
                printf("执行失败: %s\n", sql_exec_errmsg(exec));
            }
            sql_node_free(node);
        }
    }

    /* 4. 更新数据 */
    printf("\n--- 更新数据 ---\n");
    node = sql_parse_one("UPDATE users SET name = 'Alicia' WHERE id = 1");
    if (node) {
        sql_result_t *result = sql_exec(exec, node);
        if (result) {
            print_result(result);
            sql_result_free(result);
        }
        sql_node_free(node);
    }

    /* 验证更新 */
    printf("\n验证更新结果:\n");
    node = sql_parse_one("SELECT * FROM users WHERE id = 1");
    if (node) {
        sql_result_t *result = sql_exec(exec, node);
        if (result) {
            print_result(result);
            sql_result_free(result);
        }
        sql_node_free(node);
    }

    /* 5. 删除数据 */
    printf("\n--- 删除数据 ---\n");
    node = sql_parse_one("DELETE FROM users WHERE id = 5");
    if (node) {
        sql_result_t *result = sql_exec(exec, node);
        if (result) {
            print_result(result);
            sql_result_free(result);
        }
        sql_node_free(node);
    }

    /* 验证删除 */
    printf("\n验证删除结果:\n");
    node = sql_parse_one("SELECT * FROM users");
    if (node) {
        sql_result_t *result = sql_exec(exec, node);
        if (result) {
            print_result(result);
            sql_result_free(result);
        }
        sql_node_free(node);
    }

    /* 6. 删除表 */
    printf("\n--- 删除表 ---\n");
    node = sql_parse_one("DROP TABLE users");
    if (node) {
        sql_exec_ddl(exec, node);
        sql_node_free(node);
        printf("表已删除\n");
    }

    sql_exec_destroy(exec);
    kv_close(db);
    printf("\n");
}

/* ─────────────────────────────────────────────────────────────────
 * 事务 API 示例
 * ───────────────────────────────────────────────────────────────── */

void transaction_api_demo(void)
{
    printf("=== 事务 API 示例 ===\n\n");

    kv_t *db = kv_open("./txn_demo.db");

    /* 开启事务 */
    txn_id_t txn = txn_begin(db);
    printf("开启事务: txn_id=%llu\n", (unsigned long long)txn);

    /* 在事务中执行操作 */
    kv_put(db, "key1", "value1", 6);
    kv_put(db, "key2", "value2", 6);
    printf("插入 key1, key2\n");

    /* 提交事务 */
    txn_commit(db, txn);
    printf("事务已提交\n\n");

    /* 开启新事务并回滚 */
    txn = txn_begin(db);
    printf("开启新事务: txn_id=%llu\n", (unsigned long long)txn);

    kv_put(db, "key3", "rollback_value", 15);
    printf("插入 key3 (将被回滚)\n");

    /* 回滚事务 */
    txn_rollback(db, txn);
    printf("事务已回滚\n");

    /* 验证 key3 不存在 */
    char buf[64];
    size_t len = kv_get(db, "key3", buf, sizeof(buf));
    printf("key3 存在: %s\n\n", len > 0 ? "是" : "否");

    kv_close(db);
}

/* ─────────────────────────────────────────────────────────────────
 * 性能 API 示例
 * ───────────────────────────────────────────────────────────────── */

void performance_api_demo(void)
{
    printf("=== 性能 API 示例 ===\n\n");

    /* CPU 信息 */
    int cpus = get_cpu_count();
    printf("CPU 核心数: %d\n", cpus);

    /* SIMD 支持 */
    if (simd_is_supported()) {
        printf("SIMD 支持: 是 (%s)\n", simd_get_type());
    } else {
        printf("SIMD 支持: 否\n");
    }

    /* 内存信息 */
    uint64_t mem = get_available_memory();
    printf("可用内存: %.2f GB\n", mem / (1024.0 * 1024 * 1024));

    /* 线程估算 */
    int cpu_workers = estimate_optimal_workers(0);  /* CPU 密集型 */
    int io_workers = estimate_optimal_workers(1);   /* IO 密集型 */
    printf("推荐工作线程数 (CPU 密集): %d\n", cpu_workers);
    printf("推荐工作线程数 (IO 密集): %d\n", io_workers);

    /* 创建并行执行器 */
    parallel_config_t config = {
        .num_workers = cpus,
        .chunk_size = 1024,
        .auto_tuning = true
    };
    parallel_executor_t *executor = parallel_executor_create(&config);
    if (executor) {
        printf("并行执行器创建成功，工作线程数: %d\n",
               parallel_executor_workers(executor));
        parallel_executor_destroy(executor);
    }

    printf("\n");
}

/* ─────────────────────────────────────────────────────────────────
 * 主函数
 * ───────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║     Build My DB - API 使用示例                   ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* KV API 演示 */
    kv_api_demo();

    /* SQL API 演示 */
    sql_api_demo();

    /* 事务 API 演示 */
    transaction_api_demo();

    /* 性能 API 演示 */
    performance_api_demo();

    printf("示例完成！\n");
    return 0;
}