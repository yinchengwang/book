/**
 * @file main.c
 * @brief 备份命令行工具
 *
 * 用法: db_backup <db_path> <backup_dir>
 *
 * 将数据库文件备份到指定目录，生成 meta.json 元数据文件。
 */
#include <stdio.h>
#include "db/backup.h"

static void print_usage(const char *prog) {
    fprintf(stderr, "用法: %s <db_path> <backup_dir>\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "将数据库备份到指定目录。\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "参数:\n");
    fprintf(stderr, "  db_path      数据库文件路径 (*.db)\n");
    fprintf(stderr, "  backup_dir   备份目标目录\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "示例:\n");
    fprintf(stderr, "  %s ./data/test_kv.db /backup/20260719/\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *db_path = argv[1];
    const char *backup_dir = argv[2];

    int ret = db_backup(db_path, backup_dir);
    if (ret != 0) {
        fprintf(stderr, "备份失败!\n");
        return 1;
    }

    printf("备份完成: %s\n", backup_dir);
    return 0;
}