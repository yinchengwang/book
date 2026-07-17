#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "infra/infra.h"
#include "infra/model.h"

int cmd_embed(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "用法: infra embed <text> --model <path> [--json]\n");
        return 1;
    }
    const char* text = argv[1];
    const char* model_path = "reference/models/minilm-l6-q4_k_m.gguf";
    int json_output = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) model_path = argv[++i];
        else if (strcmp(argv[i], "--json") == 0) json_output = 1;
    }

    infra_init();
    infra_model_t* m = infra_model_load(model_path, INFRA_MODEL_GGUF);
    if (!m) {
        fprintf(stderr, "加载模型失败\n");
        return 1;
    }
    float* emb = NULL;
    int dim = 0;
    if (infra_embed(m, &text, 1, &emb, &dim) != INFRA_OK) {
        infra_model_free(m);
        return 1;
    }
    if (json_output) {
        printf("{\"dim\":%d,\"vector\":[", dim);
        for (int i = 0; i < dim; i++) {
            printf("%s%.6f", i ? "," : "", emb[i]);
        }
        printf("]}\n");
    } else {
        printf("维度: %d\n向量: [", dim);
        for (int i = 0; i < dim && i < 10; i++) printf("%.4f ", emb[i]);
        printf("...]\n");
    }
    infra_embed_free(emb, 1);
    infra_model_free(m);
    return 0;
}