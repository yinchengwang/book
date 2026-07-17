#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "kbase_index.h"
#include "kbase_search.h"
#include "infra/infra.h"
#include "infra/model.h"

int cmd_search(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "用法: kbase search <query> [--top-k 10]\n");
        return 1;
    }
    const char* query = argv[1];
    int top_k = 10;
    const char* model_path = "reference/models/minilm-l6-q4_k_m.gguf";

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--top-k") == 0 && i + 1 < argc) top_k = atoi(argv[++i]);
        else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) model_path = argv[++i];
    }

    infra_init();
    infra_model_t* m = infra_model_load(model_path, INFRA_MODEL_GGUF);
    if (!m) {
        fprintf(stderr, "加载模型失败\n");
        return 1;
    }
    kbase_index_t* idx = kbase_index_create("learning/notes");
    kbase_index_scan(idx);
    /* Phase 1 简化：从索引文件加载向量（实际需要 kbase_index_load） */
    /* 此处使用占位：扫描后手动构建 */

    int n = 0;
    kbase_result_t* results = kbase_search(idx, m, query, top_k, &n);
    if (!results || n == 0) {
        printf("无结果（请先运行 kbase index）\n");
        kbase_index_destroy(idx);
        infra_model_free(m);
        return 0;
    }
    printf("搜索结果（top-%d）：\n", n);
    for (int i = 0; i < n; i++) {
        printf("[%d] %s (score: %.3f)\n", results[i].rank,
               results[i].note->title, results[i].score);
        printf("    📄 %s\n", results[i].note->path);
    }
    kbase_search_free(results, n);
    kbase_index_destroy(idx);
    infra_model_free(m);
    return 0;
}