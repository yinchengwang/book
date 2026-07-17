/**
 * @file main.c
 * @brief MiniVecDB CLI 主程序入口
 *
 * 用法:
 *   vdb_cli <command> [options] [args]
 *   vdb_cli --help
 *   vdb_cli --version
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "vdb_cli.h"

/* ========================================================================
 * 命令行选项
 * ======================================================================== */

static struct option long_options[] = {
    {"help",       no_argument,       0, 'h'},
    {"version",    no_argument,       0, 'V'},
    {"data-dir",   required_argument, 0, 'd'},
    {"json",       no_argument,       0, 'j'},
    {"silent",     no_argument,       0, 's'},
    {0, 0, 0, 0}
};

static const char *short_options = "hd:jV";

/* ========================================================================
 * 使用说明
 * ======================================================================== */

static void print_usage(const char *prog) {
    printf("用法: %s [选项] <命令> [命令参数]\n\n", prog);
    printf("选项:\n");
    printf("  -h, --help          显示帮助信息\n");
    printf("  -V, --version       显示版本信息\n");
    printf("  -d, --data-dir      数据目录（默认: ./vdb_data）\n");
    printf("  -j, --json          JSON 输出模式\n");
    printf("  -s, --silent        静默模式\n\n");
    printf("命令:\n");
    printf("  create              创建集合\n");
    printf("  drop                删除集合\n");
    printf("  list                列出所有集合\n");
    printf("  info                显示集合信息\n");
    printf("  insert              插入向量\n");
    printf("  query               查询向量\n");
    printf("  delete              删除向量\n");
    printf("  benchmark           性能测试\n");
    printf("  shell               进入交互式模式\n\n");
    printf("示例:\n");
    printf("  %s create --collection test --dim 128\n", prog);
    printf("  %s insert -c test --vector 1.0,2.0,3.0\n", prog);
    printf("  %s query -c test --vector 1.0,2.0,3.0 -k 10\n", prog);
    printf("  %s shell\n", prog);
}

/* ========================================================================
 * 主程序
 * ======================================================================== */

int main(int argc, char *argv[]) {
    const char *data_dir = NULL;
    bool json_mode = false;
    bool silent_mode = false;
    const char *command = NULL;

    /* 解析全局选项 */
    int opt;
    int option_index = 0;

    optind = 1;  /* 重置选项索引 */
    /* 注意：不设置 opterr = 0，让 getopt_long 自动处理未知选项 */
    /* 只处理全局选项，命令参数由 vdb_cli_exec 处理 */

    /* 先跳过任何看起来像选项的参数，直到找到第一个非选项参数（命令） */
    while (optind < argc && argv[optind][0] == '-') {
        const char *arg = argv[optind];

        /* 检查是否是全局选项 */
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
            vdb_cli_print_version();
            return 0;
        } else if (strcmp(arg, "-d") == 0 && optind + 1 < argc) {
            data_dir = argv[optind + 1];
            optind += 2;
        } else if (strncmp(arg, "-d", 2) == 0) {
            data_dir = arg + 2;
            optind++;
        } else if (strcmp(arg, "-j") == 0 || strcmp(arg, "--json") == 0) {
            json_mode = true;
            optind++;
        } else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--silent") == 0) {
            silent_mode = true;
            optind++;
        } else {
            /* 不是全局选项，说明第一个参数是命令，跳出循环 */
            break;
        }
    }

    /* 获取命令 */
    if (optind < argc) {
        command = argv[optind++];
    } else {
        /* 无命令时进入 shell 模式 */
        command = "shell";
    }

    /* 构建 CLI 配置 */
    vdb_cli_config_t config;
    if (data_dir) {
        config.data_dir = data_dir;
    } else {
        config.data_dir = "./vdb_data";
    }
    config.prompt = "vdb> ";
    config.history_size = 100;
    config.echo = !silent_mode;
    config.show_timing = !silent_mode;
    config.json_output = json_mode;
    config.color_output = !json_mode && !silent_mode;

    /* 创建 CLI */
    vdb_cli_t *cli = vdb_cli_create(&config);
    if (!cli) {
        fprintf(stderr, "错误: 无法创建 CLI 实例\n");
        return 1;
    }

    if (json_mode) {
        vdb_cli_set_output_format(cli, VDB_OUTPUT_JSON);
    } else if (silent_mode) {
        vdb_cli_set_output_format(cli, VDB_OUTPUT_SILENT);
    }

    /* 执行命令 */
    int ret = vdb_cli_exec(cli, command, argc - optind, &argv[optind]);

    /* 清理 */
    vdb_cli_destroy(cli);

    return ret;
}
