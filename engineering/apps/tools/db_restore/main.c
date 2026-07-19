/**
 * @file main.c
 * @brief 恢复命令行工具
 *
 * 用法: db_restore <backup_dir> <db_dir>
 *
 * 从备份目录恢复数据库到指定目标目录。
 */
#include <stdio.h>
#include "db/backup.h"

static void print_usage(const char *prog) {
    fprintf(stderr, "用法: %s <backup_dir> <db_dir>\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "从备份目录恢复数据库。\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "参数:\n");
    fprintf(stderr, "  backup_dir   备份源目录\n");
    fprintf(stderr, "  db_dir       目标数据库目录\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "示例:\n");
    fprintf(stderr, "  %s /backup/20260719/ ./data/\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *backup_dir = argv[1];
    const char *db_dir = argv[2];

    int ret = db_restore(backup_dir, db_dir);
    if (ret != 0) {
        fprintf(stderr, "恢复失败!\n");
        return 1;
    }

    printf("恢复完成: %s\n", db_dir);
    return 0;
}