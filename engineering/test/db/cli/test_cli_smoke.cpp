/*
 * test_cli_smoke.cpp - CLI 烟测（T10：CLI 重接线到 canonical execute_sql）
 *
 * 通过 db_cli_exec 验证 CLI 已正确接入 sql_engine 的 execute_sql(sql, NULL) 主路径：
 *   CREATE TABLE → INSERT → SELECT → 语法错误（非零返回）
 *
 * cfg.prompt="" 防止任何提示被打印到 gtest 输出；TearDown 删除临时数据库文件。
 */

#include <gtest/gtest.h>
#include "db/cli/cli.h"
#include <cstdio>

TEST(CliSmoke, ExecCreateInsertSelect) {
    /* 删除可能残留的临时 db 文件 */
    std::remove("./cli_smoke.db");

    db_cli_config_t cfg = {};
    cfg.db_path = "./cli_smoke.db";
    cfg.prompt = "";
    cfg.echo = false;
    cfg.json_output = false;
    cfg.show_timing = false;

    db_cli_t *cli = db_cli_create(&cfg);
    ASSERT_NE(cli, nullptr);

    EXPECT_EQ(db_cli_exec(cli, "CREATE TABLE c (id INT)"), 0);
    EXPECT_EQ(db_cli_exec(cli, "INSERT INTO c VALUES (1)"), 0);
    EXPECT_EQ(db_cli_exec(cli, "SELECT id FROM c"), 0);
    /* 语法错误必须非零返回 */
    EXPECT_NE(db_cli_exec(cli, "SELEKT nonsense"), 0);

    db_cli_destroy(cli);

    /* 清理临时数据库文件 */
    std::remove("./cli_smoke.db");
}
