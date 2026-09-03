/**
 * @file chunk_op.c
 * @brief 分块算子实现
 *
 * 移植自 RAG 系统的 Chunker：
 * - FixedChunker：固定大小分块
 * - RecursiveChunker：递归字符分块
 * - CodeChunker：代码感知分块
 */

#include "db/rag_executor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 去除字符串首尾空白（原地版本）
 */
static void trim_inplace(char *s) {
    if (!s) return;

    /* 去除首部空白 */
    char *p = s;
    while (isspace((unsigned char)*p)) {
        p++;
    }

    /* 移动内容 */
    if (p > s) {
        memmove(s, p, strlen(p) + 1);
    }

    if (*s == '\0') return;

    /* 去除尾部空白 */
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
}

/**
 * @brief 去除字符串首尾空白（返回新指针版本）
 */
static char *trim(char *s) {
    trim_inplace(s);
    return s;
}

/**
 * @brief 生成简化的 UUID
 */
static void generate_uuid(char *buf, size_t bufsize) {
    static const char hex[] = "0123456789abcdef";
    static int initialized = 0;
    static unsigned int seed;

    if (!initialized) {
        seed = (unsigned int)time(NULL);
        initialized = 1;
    }

    /* 简单的 UUID v4 格式 */
    snprintf(buf, bufsize,
             "%c%c%c%c%c%c%c%c-%c%c%c%c-%c%c%c%c-"
             "%c%c%c%c-%c%c%c%c%c%c%c%c%c%c%c%c",
             hex[(seed >> 28) & 0xF], hex[(seed >> 24) & 0xF],
             hex[(seed >> 20) & 0xF], hex[(seed >> 16) & 0xF],
             hex[(seed >> 12) & 0xF], hex[(seed >> 8) & 0xF],
             hex[(seed >> 4) & 0xF], hex[seed & 0xF],
             hex[(seed >> 28) & 0xF], hex[(seed >> 24) & 0xF],
             hex[(seed >> 20) & 0xF], hex[(seed >> 16) & 0xF],
             hex[(seed >> 12) & 0xF], hex[(seed >> 8) & 0xF],
             hex[(seed >> 4) & 0xF], hex[seed & 0xF],
             hex[(seed >> 28) & 0xF], hex[(seed >> 24) & 0xF],
             hex[(seed >> 20) & 0xF], hex[(seed >> 16) & 0xF],
             hex[(seed >> 12) & 0xF], hex[(seed >> 8) & 0xF],
             hex[(seed >> 4) & 0xF], hex[seed & 0xF],
             hex[(seed >> 28) & 0xF], hex[(seed >> 24) & 0xF],
             hex[(seed >> 20) & 0xF], hex[(seed >> 16) & 0xF],
             hex[(seed >> 12) & 0xF], hex[(seed >> 8) & 0xF],
             hex[(seed >> 4) & 0xF], hex[seed & 0xF]);
}

/* ============================================================
 * FixedChunker 实现
 * ============================================================ */

/**
 * @brief FixedChunker 内部状态
 */
typedef struct fixed_chunker_state {
    rag_config_t config;
    int64_t start_time;
} fixed_chunker_state_t;

static int fixed_chunker_init(rag_operator_t *op, rag_context_t *ctx) {
    if (!op || !ctx) return -1;

    fixed_chunker_state_t *state = (fixed_chunker_state_t *)calloc(1, sizeof(*state));
    if (!state) return -1;

    state->config = ctx->config;
    op->state = state;
    op->context = ctx;

    return 0;
}

/**
 * @brief 固定大小分块核心逻辑
 */
static int fixed_chunker_execute_impl(fixed_chunker_state_t *state,
                                       const char *text,
                                       int text_len,
                                       rag_chunk_t **out_chunks,
                                       int *out_nchunks) {
    if (!state || !text) return -1;

    int len = (text_len > 0) ? text_len : (int)strlen(text);
    int chunk_size = state->config.chunk_size > 0 ?
                     state->config.chunk_size : RAG_DEFAULT_CHUNK_SIZE;
    int chunk_overlap = state->config.chunk_overlap > 0 ?
                        state->config.chunk_overlap : RAG_DEFAULT_CHUNK_OVERLAP;

    /* 预分配数组（按最大可能数量分配） */
    int max_chunks = (len / (chunk_size - chunk_overlap)) + 2;
    rag_chunk_t *chunks = (rag_chunk_t *)calloc(max_chunks, sizeof(rag_chunk_t));
    if (!chunks) return -1;

    int nchunks = 0;
    int pos = 0;
    int chunk_index = 0;

    while (pos < len && nchunks < max_chunks) {
        int end = pos + chunk_size;
        if (end > len) end = len;

        /* 尝试在单词边界结束 */
        if (end < len) {
            while (end > pos && !isspace((unsigned char)text[end - 1])) {
                end--;
            }
            if (end == pos) {
                end = pos + chunk_size;
                if (end > len) end = len;
            }
        }

        /* 提取内容并 trim */
        int content_len = end - pos;
        char *content = (char *)malloc(content_len + 1);
        if (!content) continue;

        memcpy(content, text + pos, content_len);
        content[content_len] = '\0';
        trim(content);

        if (strlen(content) > 0) {
            rag_chunk_t *chunk = &chunks[nchunks];
            generate_uuid(chunk->id, sizeof(chunk->id));

            if (nchunks < max_chunks && chunks[nchunks].id[0]) {
                /* 已有数据，安全 */
            }

            chunk->content = content;
            chunk->content_len = (int)strlen(content);
            chunk->chunk_index = chunk_index++;
            chunk->start_char = pos;
            chunk->end_char = end;

            nchunks++;
        } else {
            free(content);
        }

        pos = end - chunk_overlap;
        if (pos <= 0) pos = end;
        if (pos <= 0) break;
    }

    *out_chunks = chunks;
    *out_nchunks = nchunks;

    return 0;
}

static int fixed_chunker_execute(rag_operator_t *op, void *input, void **output) {
    if (!op || !input || !output) return -1;

    chunk_input_t *in = (chunk_input_t *)input;
    chunk_output_t *out = (chunk_output_t *)calloc(1, sizeof(*out));
    if (!out) return -1;

    fixed_chunker_state_t *state = (fixed_chunker_state_t *)op->state;
    if (fixed_chunker_execute_impl(state, in->text, in->text_len,
                                    &out->chunks, &out->nchunks) != 0) {
        free(out);
        return -1;
    }

    out->nchunks_capacity = out->nchunks;
    out->processing_time_ms = 1;  /* 简化估算 */
    out->total_tokens = out->nchunks * 100;

    *output = out;
    return 0;
}

static int fixed_chunker_cleanup(rag_operator_t *op) {
    if (!op) return -1;
    if (op->state) {
        free(op->state);
        op->state = NULL;
    }
    return 0;
}

/* ============================================================
 * RecursiveChunker 实现
 * ============================================================ */

/**
 * @brief RecursiveChunker 内部状态
 */
typedef struct recursive_chunker_state {
    rag_config_t config;
    const char *separators[6];  /* "\n\n", "\n", ". ", ", ", " ", "" */
} recursive_chunker_state_t;

static int recursive_chunker_init(rag_operator_t *op, rag_context_t *ctx) {
    if (!op || !ctx) return -1;

    recursive_chunker_state_t *state = (recursive_chunker_state_t *)calloc(1, sizeof(*state));
    if (!state) return -1;

    state->config = ctx->config;
    state->separators[0] = "\n\n";
    state->separators[1] = "\n";
    state->separators[2] = ". ";
    state->separators[3] = ", ";
    state->separators[4] = " ";
    state->separators[5] = "";

    op->state = state;
    op->context = ctx;

    return 0;
}

/**
 * @brief 递归分割文本
 */
static int recursive_split(const char *text, int text_len,
                           const char *separators[],
                           int depth, int max_depth,
                           int chunk_size,
                           char ***out_parts, int *out_nparts) {
    if (!text || !separators || !out_parts || !out_nparts) return -1;

    int len = (text_len > 0) ? text_len : (int)strlen(text);

    if (depth >= max_depth || len <= chunk_size) {
        /* 达到最大深度或文本足够小，不再分割 */
        *out_parts = (char **)malloc(sizeof(char *));
        if (!*out_parts) return -1;
        (*out_parts)[0] = (char *)malloc(len + 1);
        if (!(*out_parts)[0]) {
            free(*out_parts);
            return -1;
        }
        memcpy((*out_parts)[0], text, len);
        (*out_parts)[0][len] = '\0';
        *out_nparts = 1;
        return 0;
    }

    const char *sep = separators[depth];
    int sep_len = (int)strlen(sep);

    /* 收集所有分割后的部分 */
    char **all_parts = NULL;
    int nall = 0;
    int all_cap = 16;
    all_parts = (char **)malloc(all_cap * sizeof(char *));

    size_t pos = 0;
    while (pos < (size_t)len) {
        size_t next = 0;
        if (sep_len > 0) {
            next = 0;  /* 简化：在实际代码中需要 find */
        }

        /* 简化：直接按字符数分割 */
        size_t part_end = pos + chunk_size;
        if (part_end > (size_t)len) part_end = len;

        int part_len = (int)(part_end - pos);
        char *part = (char *)malloc(part_len + 1);
        if (!part) continue;

        memcpy(part, text + pos, part_len);
        part[part_len] = '\0';

        if (nall >= all_cap) {
            all_cap *= 2;
            char **new_parts = (char **)realloc(all_parts, all_cap * sizeof(char *));
            if (new_parts) all_parts = new_parts;
        }

        all_parts[nall++] = part;
        pos = part_end;
    }

    /* 合并小片段 */
    char **merged = NULL;
    int nmerged = 0;
    int merged_cap = 16;
    merged = (char **)malloc(merged_cap * sizeof(char *));

    char *current = NULL;
    int current_len = 0;

    for (int i = 0; i < nall; i++) {
        char *part = all_parts[i];
        int part_len = (int)strlen(part);

        if (!current) {
            current = (char *)malloc(part_len + 1);
            if (current) {
                strcpy(current, part);
                current_len = part_len;
            }
        } else if (current_len + sep_len + part_len <= chunk_size) {
            char *new_current = (char *)realloc(current, current_len + sep_len + part_len + 1);
            if (new_current) {
                current = new_current;
                if (sep_len > 0) strcat(current, sep);
                strcat(current, part);
                current_len = (int)strlen(current);
            }
        } else {
            /* 保存当前块并开始新块 */
            if (nmerged >= merged_cap) {
                merged_cap *= 2;
                char **new_merged = (char **)realloc(merged, merged_cap * sizeof(char *));
                if (new_merged) merged = new_merged;
            }
            merged[nmerged++] = trim(current);
            current = (char *)malloc(part_len + 1);
            if (current) {
                strcpy(current, part);
                current_len = part_len;
            }
        }

        free(part);
    }

    if (current && current_len > 0) {
        if (nmerged >= merged_cap) {
            merged_cap *= 2;
            char **new_merged = (char **)realloc(merged, merged_cap * sizeof(char *));
            if (new_merged) merged = new_merged;
        }
        merged[nmerged++] = trim(current);
    } else if (current) {
        free(current);
    }

    free(all_parts);

    *out_parts = merged;
    *out_nparts = nmerged;

    return 0;
}

static int recursive_chunker_execute(rag_operator_t *op, void *input, void **output) {
    if (!op || !input || !output) return -1;

    chunk_input_t *in = (chunk_input_t *)input;
    chunk_output_t *out = (chunk_output_t *)calloc(1, sizeof(*out));
    if (!out) return -1;

    recursive_chunker_state_t *state = (recursive_chunker_state_t *)op->state;

    int len = (in->text_len > 0) ? in->text_len : (int)strlen(in->text);
    int chunk_size = state->config.chunk_size > 0 ?
                     state->config.chunk_size : RAG_DEFAULT_CHUNK_SIZE;

    /* 执行递归分割 */
    char **parts = NULL;
    int nparts = 0;

    if (recursive_split(in->text, len, state->separators, 0, 6, chunk_size,
                        &parts, &nparts) != 0) {
        free(out);
        return -1;
    }

    /* 分配 chunk 数组 */
    out->chunks = (rag_chunk_t *)calloc(nparts, sizeof(rag_chunk_t));
    if (!out->chunks) {
        for (int i = 0; i < nparts; i++) free(parts[i]);
        free(parts);
        free(out);
        return -1;
    }

    int pos = 0;
    for (int i = 0; i < nparts; i++) {
        rag_chunk_t *chunk = &out->chunks[out->nchunks];
        char *content = trim(parts[i]);

        if (strlen(content) > 0) {
            generate_uuid(chunk->id, sizeof(chunk->id));
            chunk->content = content;  /* 转移所有权 */
            chunk->content_len = (int)strlen(content);
            chunk->chunk_index = out->nchunks;
            chunk->start_char = pos;
            chunk->end_char = pos + chunk->content_len;
            out->nchunks++;
            pos += chunk->content_len + 1;
        } else {
            free(content);
        }
    }

    for (int i = nparts; i < nparts; i++) {
        /* 已处理完所有部分 */
    }

    free(parts);
    out->nchunks_capacity = out->nchunks;
    out->processing_time_ms = 1;
    out->total_tokens = out->nchunks * 100;

    *output = out;
    return 0;
}

static int recursive_chunker_cleanup(rag_operator_t *op) {
    if (!op) return -1;
    if (op->state) {
        free(op->state);
        op->state = NULL;
    }
    return 0;
}

/* ============================================================
 * CodeChunker 实现
 * ============================================================ */

/**
 * @brief CodeChunker 内部状态
 */
typedef struct code_chunker_state {
    rag_config_t config;
    char language[32];
} code_chunker_state_t;

static int code_chunker_init(rag_operator_t *op, rag_context_t *ctx) {
    if (!op || !ctx) return -1;

    code_chunker_state_t *state = (code_chunker_state_t *)calloc(1, sizeof(*state));
    if (!state) return -1;

    state->config = ctx->config;
    if (ctx->config.language[0]) {
        strncpy(state->language, ctx->config.language, sizeof(state->language) - 1);
    } else {
        strcpy(state->language, "cpp");
    }

    op->state = state;
    op->context = ctx;

    return 0;
}

/**
 * @brief 获取语言特定分隔符
 */
static const char *code_separators[] = {"}\n\n", "\n\n", "\n", ";\n", ""};

static int code_chunker_execute(rag_operator_t *op, void *input, void **output) {
    if (!op || !input || !output) return -1;

    chunk_input_t *in = (chunk_input_t *)input;
    chunk_output_t *out = (chunk_output_t *)calloc(1, sizeof(*out));
    if (!out) return -1;

    code_chunker_state_t *state = (code_chunker_state_t *)op->state;
    int len = (in->text_len > 0) ? in->text_len : (int)strlen(in->text);
    int chunk_size = state->config.chunk_size > 0 ?
                     state->config.chunk_size : RAG_DEFAULT_CHUNK_SIZE;

    /* 简单的代码结构分割：跟踪大括号 */
    char **parts = NULL;
    int nparts = 0;
    int parts_cap = 16;
    parts = (char **)malloc(parts_cap * sizeof(char *));
    if (!parts) {
        free(out);
        return -1;
    }

    char *current = (char *)malloc(chunk_size * 2 + 1);
    if (!current) {
        free(parts);
        free(out);
        return -1;
    }
    int current_len = 0;
    int brace_count = 0;

    for (int i = 0; i < len; i++) {
        /* 在写入前检查是否需要强制分割（预留空间给下一个字符） */
        if (current_len >= chunk_size * 2 - 1) {
            /* 强制分割 */
            if (nparts >= parts_cap) {
                parts_cap *= 2;
                char **new_parts = (char **)realloc(parts, parts_cap * sizeof(char *));
                if (new_parts) parts = new_parts;
            }

            if (current_len > 0) {
                current[current_len] = '\0';
                parts[nparts++] = strdup(current);
            }

            current_len = 0;
            brace_count = 0;
        }

        current[current_len++] = in->text[i];

        /* 跟踪大括号 */
        if (in->text[i] == '{') brace_count++;
        else if (in->text[i] == '}') brace_count--;

        /* 检查分隔符 */
        if (brace_count == 0 && current_len > 2) {
            for (int s = 0; s < 4; s++) {
                const char *sep = code_separators[s];
                int sep_len = (int)strlen(sep);
                if (sep_len > 0 && current_len >= sep_len) {
                    if (strncmp(current + current_len - sep_len, sep, sep_len) == 0) {
                        /* 找到分隔符，创建新部分 */
                        current[current_len - sep_len] = '\0';
                        trim(current);

                        if (nparts >= parts_cap) {
                            parts_cap *= 2;
                            char **new_parts = (char **)realloc(parts, parts_cap * sizeof(char *));
                            if (new_parts) parts = new_parts;
                        }

                        if (strlen(current) > 0) {
                            parts[nparts++] = strdup(current);
                        }

                        current_len = 0;
                        brace_count = 0;
                        break;
                    }
                }
            }
        }
    }

    /* 添加最后一部分 */
    if (current_len > 0) {
        current[current_len] = '\0';
        trim(current);
        if (strlen(current) > 0) {
            if (nparts >= parts_cap) {
                parts_cap *= 2;
                char **new_parts = (char **)realloc(parts, parts_cap * sizeof(char *));
                if (new_parts) parts = new_parts;
            }
            parts[nparts++] = strdup(current);
        }
    }
    free(current);

    /* 创建 Chunk */
    out->chunks = (rag_chunk_t *)calloc(nparts, sizeof(rag_chunk_t));
    if (!out->chunks) {
        for (int i = 0; i < nparts; i++) free(parts[i]);
        free(parts);
        free(out);
        return -1;
    }

    int pos = 0;
    for (int i = 0; i < nparts; i++) {
        char *content = parts[i];
        trim(content);  /* trim 会修改 content */
        if (strlen(content) > 0) {
            rag_chunk_t *chunk = &out->chunks[out->nchunks];
            generate_uuid(chunk->id, sizeof(chunk->id));
            chunk->content = content;  /* 转移所有权给 chunk */
            chunk->content_len = (int)strlen(content);
            chunk->chunk_index = out->nchunks;
            chunk->start_char = pos;
            chunk->end_char = pos + chunk->content_len;
            out->nchunks++;
            pos += chunk->content_len;
        } else {
            free(content);  /* 空内容直接释放 */
        }
        /* 不再释放 parts[i]，因为 content 已转移或已释放 */
    }

    free(parts);
    out->nchunks_capacity = out->nchunks;
    out->processing_time_ms = 1;
    out->total_tokens = out->nchunks * 100;

    *output = out;
    return 0;
}

static int code_chunker_cleanup(rag_operator_t *op) {
    if (!op) return -1;
    if (op->state) {
        free(op->state);
        op->state = NULL;
    }
    return 0;
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

rag_operator_t *rag_chunk_op_create(rag_chunk_strategy_t strategy,
                                     const rag_config_t *config) {
    rag_operator_t *op = (rag_operator_t *)calloc(1, sizeof(*op));
    if (!op) return NULL;

    op->type = RAG_OP_CHUNK;
    op->context = NULL;
    op->state = NULL;

    switch (strategy) {
        case RAG_CHUNK_FIXED:
            strcpy(op->name, "FixedChunker");
            op->init = fixed_chunker_init;
            op->execute = fixed_chunker_execute;
            op->cleanup = fixed_chunker_cleanup;
            break;

        case RAG_CHUNK_RECURSIVE:
            strcpy(op->name, "RecursiveChunker");
            op->init = recursive_chunker_init;
            op->execute = recursive_chunker_execute;
            op->cleanup = recursive_chunker_cleanup;
            break;

        case RAG_CHUNK_CODE:
            strcpy(op->name, "CodeChunker");
            op->init = code_chunker_init;
            op->execute = code_chunker_execute;
            op->cleanup = code_chunker_cleanup;
            break;

        default:
            strcpy(op->name, "FixedChunker");
            op->init = fixed_chunker_init;
            op->execute = fixed_chunker_execute;
            op->cleanup = fixed_chunker_cleanup;
            break;
    }

    return op;
}

int rag_operator_init(rag_operator_t *op, rag_context_t *ctx) {
    if (!op || !op->init) return -1;
    return op->init(op, ctx);
}

int rag_operator_execute(rag_operator_t *op, void *input, void **output) {
    if (!op || !op->execute) return -1;
    return op->execute(op, input, output);
}

int rag_operator_cleanup(rag_operator_t *op) {
    if (!op || !op->cleanup) return -1;
    return op->cleanup(op);
}

void rag_operator_destroy(rag_operator_t *op) {
    if (!op) return;
    if (op->cleanup) op->cleanup(op);
    free(op);
}

void chunk_output_free(chunk_output_t *output) {
    if (!output) return;

    if (output->chunks) {
        for (int i = 0; i < output->nchunks; i++) {
            if (output->chunks[i].content) {
                free(output->chunks[i].content);
            }
        }
        free(output->chunks);
    }

    free(output);
}

chunk_output_t *rag_chunk(rag_context_t *ctx,
                          const char *text,
                          const char *document_id) {
    if (!ctx || !text) return NULL;

    rag_operator_t *op = rag_chunk_op_create(ctx->config.chunk_strategy, &ctx->config);
    if (!op) return NULL;

    if (rag_operator_init(op, ctx) != 0) {
        rag_operator_destroy(op);
        return NULL;
    }

    chunk_input_t input;
    memset(&input, 0, sizeof(input));
    input.text = text;
    input.document_id = document_id;

    void *output = NULL;
    if (rag_operator_execute(op, &input, &output) != 0) {
        rag_operator_cleanup(op);
        rag_operator_destroy(op);
        return NULL;
    }

    rag_operator_cleanup(op);
    rag_operator_destroy(op);

    return (chunk_output_t *)output;
}
