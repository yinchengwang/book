#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 命令处理函数声明 */
extern int cmd_index(int argc, char** argv);
extern int cmd_search(int argc, char** argv);
extern int cmd_embed(int argc, char** argv);
extern int cmd_help(int argc, char** argv);

typedef struct {
    const char* name;
    int (*handler)(int argc, char** argv);
    const char* description;
} command_t;

static command_t commands[] = {
    {"index",   cmd_index,   "构建笔记索引"},
    {"search",  cmd_search,  "语义搜索笔记"},
    {"embed",   cmd_embed,   "推理引擎 Embedding"},
    {"help",    cmd_help,    "显示帮助"},
    {NULL, NULL, NULL},
};

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "用法: kbase <command> [options]\n");
        fprintf(stderr, "命令: index, search, embed, help\n");
        return 1;
    }
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(argv[1], commands[i].name) == 0) {
            return commands[i].handler(argc - 1, argv + 1);
        }
    }
    fprintf(stderr, "未知命令: %s\n", argv[1]);
    return 1;
}

int cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("kbase CLI - 知识库工具\n\n");
    printf("命令:\n");
    for (int i = 0; commands[i].name; i++) {
        printf("  %-10s %s\n", commands[i].name, commands[i].description);
    }
    return 0;
}