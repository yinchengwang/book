/*
 * main.c - 全文索引与倒排索引演示
 *
 * 演示内容：
 * 1. 倒排索引结构：term -> [doc_id, freq, positions]
 * 2. 简单分词（空格+标点分割，转小写）
 * 3. BM25 评分公式
 * 4. 相关度排序输出
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ── 常量定义 ── */
#define MAX_DOCS 100          /* 最大文档数 */
#define MAX_TERMS 200         /* 最大词条数 */
#define MAX_TERM_LEN 64       /* 词条最大长度 */
#define MAX_POSITIONS 50      /* 每文档最大位置数 */

/* ── 数据结构 ── */

/* 倒排记录：记录词在某个文档中的出现信息 */
typedef struct {
    int doc_id;                           /* 文档 ID */
    int freq;                             /* 词频 (TF) */
    int positions[MAX_POSITIONS];         /* 出现位置 */
    int pos_count;                        /* 位置数量 */
} posting_t;

/* 倒排索引项：词条及其倒排链表 */
typedef struct {
    char term[MAX_TERM_LEN];              /* 词条 */
    posting_t *postings;                  /* 倒排链表（动态分配） */
    int posting_count;                    /* 文档数 (DF) */
    int posting_capacity;                 /* 倒排链表容量 */
    int total_freq;                       /* 总词频 */
} inverted_entry_t;

/* 文档信息 */
typedef struct {
    int doc_id;
    int word_count;                       /* 文档词数 */
    char text[1024];                      /* 原文 */
} doc_info_t;

/* 全文索引结构 */
typedef struct {
    inverted_entry_t entries[MAX_TERMS];  /* 倒排索引 */
    int term_count;                       /* 词条总数 */
    doc_info_t docs[MAX_DOCS];            /* 文档信息 */
    int doc_count;                        /* 文档总数 */
    float avg_doc_len;                    /* 平均文档长度 */
} fulltext_index_t;

/* ── 分词函数 ── */

/* 简单分词：空格+标点分割，转小写 */
int tokenize(const char *text, char tokens[][MAX_TERM_LEN], int *positions) {
    int count = 0;
    char word[MAX_TERM_LEN];
    int pos = 0, word_pos = 0;
    const char *p = text;

    while (*p) {
        if (isspace(*p) || ispunct(*p)) {
            /* 遇到分隔符，保存当前词 */
            if (pos > 0) {
                word[pos] = '\0';
                if (strlen(word) >= 2 && count < MAX_TERMS) {
                    strcpy(tokens[count], word);
                    positions[count] = word_pos;
                    count++;
                }
                pos = 0;
            }
        } else {
            /* 收集字符并转小写 */
            if (pos < MAX_TERM_LEN - 1) {
                word[pos++] = tolower(*p);
            }
        }
        word_pos++;
        p++;
    }

    /* 处理最后一个词 */
    if (pos > 0) {
        word[pos] = '\0';
        if (strlen(word) >= 2 && count < MAX_TERMS) {
            strcpy(tokens[count], word);
            positions[count] = word_pos;
            count++;
        }
    }

    return count;
}

/* ── 倒排索引操作 ── */

/* 查找或创建词条索引 */
int find_or_create_term(fulltext_index_t *idx, const char *term) {
    for (int i = 0; i < idx->term_count; i++) {
        if (strcmp(idx->entries[i].term, term) == 0) {
            return i;
        }
    }

    /* 创建新词条 */
    if (idx->term_count < MAX_TERMS) {
        strcpy(idx->entries[idx->term_count].term, term);
        idx->entries[idx->term_count].posting_count = 0;
        idx->entries[idx->term_count].posting_capacity = 10;
        idx->entries[idx->term_count].postings = 
            (posting_t *)malloc(10 * sizeof(posting_t));
        idx->entries[idx->term_count].total_freq = 0;
        return idx->term_count++;
    }

    return -1;
}

/* 向倒排索引添加词条 */
void add_to_index(fulltext_index_t *idx, int doc_id, const char *term, int position) {
    int term_idx = find_or_create_term(idx, term);
    if (term_idx < 0) return;

    inverted_entry_t *entry = &idx->entries[term_idx];
    posting_t *posting = NULL;

    /* 查找该文档的倒排记录 */
    for (int i = 0; i < entry->posting_count; i++) {
        if (entry->postings[i].doc_id == doc_id) {
            posting = &entry->postings[i];
            break;
        }
    }

    /* 创建新的倒排记录 */
    if (!posting) {
        /* 检查是否需要扩容 */
        if (entry->posting_count >= entry->posting_capacity) {
            entry->posting_capacity *= 2;
            posting_t *new_postings = (posting_t *)realloc(entry->postings,
                entry->posting_capacity * sizeof(posting_t));
            if (!new_postings) return;
            entry->postings = new_postings;
        }
        posting = &entry->postings[entry->posting_count++];
        posting->doc_id = doc_id;
        posting->freq = 0;
        posting->pos_count = 0;
    }

    /* 更新词频和位置 */
    if (posting) {
        posting->freq++;
        if (posting->pos_count < MAX_POSITIONS) {
            posting->positions[posting->pos_count++] = position;
        }
        entry->total_freq++;
    }
}

/* 索引文档 */
void index_document(fulltext_index_t *idx, int doc_id, const char *text) {
    char (*tokens)[MAX_TERM_LEN] = malloc(MAX_TERMS * MAX_TERM_LEN);
    int *positions = malloc(MAX_TERMS * sizeof(int));
    if (!tokens || !positions) {
        free(tokens);
        free(positions);
        return;
    }

    int token_count = tokenize(text, tokens, positions);

    /* 记录文档信息 */
    idx->docs[idx->doc_count].doc_id = doc_id;
    idx->docs[idx->doc_count].word_count = token_count;
    strncpy(idx->docs[idx->doc_count].text, text, 1023);
    idx->doc_count++;

    /* 建立倒排索引 */
    for (int i = 0; i < token_count; i++) {
        add_to_index(idx, doc_id, tokens[i], positions[i]);
    }

    /* 更新平均文档长度 */
    int total_words = 0;
    for (int i = 0; i < idx->doc_count; i++) {
        total_words += idx->docs[i].word_count;
    }
    idx->avg_doc_len = (float)total_words / idx->doc_count;

    free(tokens);
    free(positions);
}

/* ── BM25 评分 ── */

/*
 * BM25 评分公式：
 * score(D, Q) = Σ IDF(qi) × (tf × (k1+1)) / (tf + k1 × (1-b+b×|D|/avgdl))
 *
 * 其中：
 * - IDF(qi) = log((N - n(qi) + 0.5) / (n(qi) + 0.5))
 * - tf: 词频
 * - k1: 饱和参数（通常 1.2-2.0）
 * - b: 长度归一化参数（通常 0.75）
 * - |D|: 文档长度
 * - avgdl: 平均文档长度
 */
float compute_bm25_score(fulltext_index_t *idx, int doc_id, const char *query) {
    char (*tokens)[MAX_TERM_LEN] = malloc(MAX_TERMS * MAX_TERM_LEN);
    int *positions = malloc(MAX_TERMS * sizeof(int));
    if (!tokens || !positions) {
        free(tokens);
        free(positions);
        return 0.0f;
    }

    int token_count = tokenize(query, tokens, positions);

    float total_score = 0.0f;
    const float k1 = 1.5f;   /* 饱和参数 */
    const float b = 0.75f;   /* 长度归一化参数 */
    const int N = idx->doc_count;

    /* 获取文档长度 */
    int doc_len = 0;
    for (int i = 0; i < idx->doc_count; i++) {
        if (idx->docs[i].doc_id == doc_id) {
            doc_len = idx->docs[i].word_count;
            break;
        }
    }

    /* 对每个查询词计算分数 */
    for (int t = 0; t < token_count; t++) {
        const char *term = tokens[t];

        /* 查找词条 */
        int term_idx = -1;
        for (int i = 0; i < idx->term_count; i++) {
            if (strcmp(idx->entries[i].term, term) == 0) {
                term_idx = i;
                break;
            }
        }

        if (term_idx < 0) continue;

        inverted_entry_t *entry = &idx->entries[term_idx];
        int n_qi = entry->posting_count;  /* 包含该词的文档数 */

        /* 计算 IDF */
        float idf = logf((N - n_qi + 0.5f) / (n_qi + 0.5f) + 1.0f);

        /* 查找该文档的词频 */
        int tf = 0;
        for (int i = 0; i < entry->posting_count; i++) {
            if (entry->postings[i].doc_id == doc_id) {
                tf = entry->postings[i].freq;
                break;
            }
        }

        if (tf == 0) continue;

        /* BM25 分数 */
        float tf_component = (tf * (k1 + 1)) / (tf + k1 * (1 - b + b * doc_len / idx->avg_doc_len));
        total_score += idf * tf_component;
    }

    free(tokens);
    free(positions);
    return total_score;
}

/* ── 搜索与排序 ── */

/* 搜索并返回 BM25 排序结果 */
int search(fulltext_index_t *idx, const char *query, int *result_docs, float *result_scores, int max_results) {
    char (*tokens)[MAX_TERM_LEN] = malloc(MAX_TERMS * MAX_TERM_LEN);
    int *positions = malloc(MAX_TERMS * sizeof(int));
    if (!tokens || !positions) {
        free(tokens);
        free(positions);
        return 0;
    }

    int token_count = tokenize(query, tokens, positions);

    if (token_count == 0) {
        free(tokens);
        free(positions);
        return 0;
    }

    /* 收集候选文档 */
    int *candidates = (int *)calloc(MAX_DOCS, sizeof(int));
    float *scores = (float *)calloc(MAX_DOCS, sizeof(float));
    if (!candidates || !scores) {
        free(tokens);
        free(positions);
        free(candidates);
        free(scores);
        return 0;
    }

    int candidate_count = 0;

    for (int t = 0; t < token_count; t++) {
        for (int i = 0; i < idx->term_count; i++) {
            if (strcmp(idx->entries[i].term, tokens[t]) == 0) {
                inverted_entry_t *entry = &idx->entries[i];
                for (int j = 0; j < entry->posting_count; j++) {
                    int doc_id = entry->postings[j].doc_id;
                    int found = 0;
                    for (int k = 0; k < candidate_count; k++) {
                        if (candidates[k] == doc_id) {
                            found = 1;
                            break;
                        }
                    }
                    if (!found && candidate_count < MAX_DOCS) {
                        candidates[candidate_count++] = doc_id;
                    }
                }
            }
        }
    }

    /* 计算 BM25 分数 */
    for (int i = 0; i < candidate_count; i++) {
        scores[i] = compute_bm25_score(idx, candidates[i], query);
    }

    /* 按分数排序（简单选择排序） */
    int result_count = candidate_count < max_results ? candidate_count : max_results;
    for (int i = 0; i < result_count; i++) {
        int best = i;
        for (int j = i + 1; j < candidate_count; j++) {
            if (scores[j] > scores[best]) {
                best = j;
            }
        }
        /* 交换 */
        if (best != i) {
            int tmp_doc = candidates[i];
            candidates[i] = candidates[best];
            candidates[best] = tmp_doc;

            float tmp_score = scores[i];
            scores[i] = scores[best];
            scores[best] = tmp_score;
        }
        result_docs[i] = candidates[i];
        result_scores[i] = scores[i];
    }

    free(tokens);
    free(positions);
    free(candidates);
    free(scores);
    return result_count;
}

/* 清理索引资源 */
void cleanup_index(fulltext_index_t *idx) {
    for (int i = 0; i < idx->term_count; i++) {
        free(idx->entries[i].postings);
    }
}

/* ── 主函数 ── */

int main(void) {
    fulltext_index_t idx = {0};

    printf("=== 全文索引与 BM25 演示 ===\n\n");

    /* 3 篇示例文档 */
    const char *docs[] = {
        "PostgreSQL is a powerful open source relational database system",
        "MySQL is an open source database management system",
        "Full text search enables fast text querying in databases"
    };

    /* 索引文档 */
    printf("【索引文档】\n");
    for (int i = 0; i < 3; i++) {
        index_document(&idx, i + 1, docs[i]);
        printf("  Doc %d: %s\n", i + 1, docs[i]);
    }
    printf("\n统计: %d 文档, %d 词条, 平均长度 %.1f\n\n",
           idx.doc_count, idx.term_count, idx.avg_doc_len);

    /* 打印倒排索引 */
    printf("【倒排索引】\n");
    for (int i = 0; i < idx.term_count && i < 10; i++) {
        inverted_entry_t *entry = &idx.entries[i];
        printf("  '%s' -> [", entry->term);
        for (int j = 0; j < entry->posting_count; j++) {
            printf("Doc%d(tf=%d)", entry->postings[j].doc_id, entry->postings[j].freq);
            if (j < entry->posting_count - 1) printf(", ");
        }
        printf("]\n");
    }
    printf("\n");

    /* 查询演示 */
    const char *queries[] = {"database", "open source", "text search"};
    for (int q = 0; q < 3; q++) {
        printf("【查询: \"%s\"】\n", queries[q]);

        int result_docs[MAX_DOCS];
        float result_scores[MAX_DOCS];
        int result_count = search(&idx, queries[q], result_docs, result_scores, 5);

        printf("  结果 (BM25 排序):\n");
        for (int i = 0; i < result_count; i++) {
            printf("  %d. Doc %d (score=%.3f)\n", i + 1, result_docs[i], result_scores[i]);
        }
        printf("\n");
    }

    cleanup_index(&idx);
    return 0;
}
