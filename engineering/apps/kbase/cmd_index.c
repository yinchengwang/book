#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "kbase_index.h"
#include "infra/infra.h"
#include "infra/model.h"

int cmd_index(int argc, char** argv) {
    const char* dir = "learning/notes";
    const char* model_path = "reference/models/minilm-l6-q4_k_m.gguf";
    const char* save_path = "kbase";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) dir = argv[++i];
        else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) model_path = argv[++i];
        else if (strcmp(argv[i], "--save") == 0 && i + 1 < argc) save_path = argv[++i];
    }

    printf("扫描笔记目录: %s\n", dir);
    infra_init();
    infra_model_t* m = infra_model_load(model_path, INFRA_MODEL_GGUF);
    if (!m) {
        fprintf(stderr, "加载模型失败: %s\n", model_path);
        return 1;
    }
    kbase_index_t* idx = kbase_index_create(dir);
    int n = kbase_index_scan(idx);
    printf("扫描到 %d 个笔记\n", n);
    printf("生成 Embedding 中...\n");
    if (kbase_index_build(idx, m) != INFRA_OK) {
        fprintf(stderr, "构建索引失败\n");
        kbase_index_destroy(idx);
        infra_model_free(m);
        return 1;
    }
    kbase_index_save(idx, save_path);
    printf("索引已保存: %s.index / %s.vec\n", save_path, save_path);
    kbase_index_destroy(idx);
    infra_model_free(m);
    return 0;
}