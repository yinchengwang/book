/*
 * lvq.c
 *
 * LVQ (Learning Vector Quantization) 实现
 *
 * LVQ 是一种有监督的向量量化算法，通过学习调整原型向量的位置，
 * 使得相同类别的向量聚在一起，不同类别的向量分开。
 *
 * 学习规则：
 * - 找到输入向量最近的原型 p
 * - 如果 label(x) == label(p): p = p + alpha * (x - p)  拉近
 * - 如果 label(x) != label(p): p = p - alpha * (x - p)  推远
 */

#include <db/index/vector_index/lvq/lvq.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

/* ── 内部宏定义 ── */
#define LVQ_DEFAULT_ALPHA 0.01f  /* 学习率 */
#define LVQ_MAX_ITERS 100       /* 最大迭代次数 */
#define LVQ_ALPHA_DECAY 0.99f    /* 学习率衰减 */

/* ── 内部结构 ── */

/* 原型向量 */
typedef struct {
    float *vector;      /* 原型向量 [dims] */
    int label;          /* 标签 */
} lvq_prototype_t;

/* LVQ 量化器结构 */
struct lvq {
    int dims;               /* 向量维度 */
    int n_prototypes;       /* 原型数量 */
    int code_size;          /* 码字大小（4字节，存索引） */

    lvq_prototype_t *prototypes;  /* 原型数组 */
    float alpha;            /* 学习率 */
    int trained;            /* 是否已训练 */
};

/* ── 内部辅助函数 ── */

/**
 * 计算欧氏距离
 */
static float compute_l2_distance(const float *v1, const float *v2, int dims)
{
    float dist = 0.0f;
    for (int i = 0; i < dims; i++) {
        float diff = v1[i] - v2[i];
        dist += diff * diff;
    }
    return dist;
}

/**
 * 初始化原型（k-means++ 初始化）
 */
static void init_prototypes(lvq_t *lvq, const float *vectors, int n_vectors,
                            const int *labels, int n_labels)
{
    /* 第一个原型：随机选择一个带 label=0 的向量 */
    for (int i = 0; i < n_vectors; i++) {
        if (labels[i] == 0) {
            memcpy(lvq->prototypes[0].vector, vectors + i * lvq->dims, lvq->dims * sizeof(float));
            lvq->prototypes[0].label = 0;
            break;
        }
    }

    /* 剩余原型：按距离加权选择 */
    float *distances = (float *)malloc(n_vectors * sizeof(float));
    int *used = (int *)calloc(n_vectors, sizeof(int));

    for (int p = 1; p < lvq->n_prototypes; p++) {
        float total_dist = 0.0f;

        /* 计算到最近原型的距离 */
        for (int i = 0; i < n_vectors; i++) {
            float min_dist = compute_l2_distance(vectors + i * lvq->dims,
                                                lvq->prototypes[0].vector, lvq->dims);
            for (int j = 1; j < p; j++) {
                float dist = compute_l2_distance(vectors + i * lvq->dims,
                                                lvq->prototypes[j].vector, lvq->dims);
                if (dist < min_dist) min_dist = dist;
            }
            distances[i] = min_dist;
            total_dist += min_dist;
        }

        /* 按概率选择 */
        float r = ((float)rand() / RAND_MAX) * total_dist;
        float cumulative = 0.0f;
        int selected = 0;

        for (int i = 0; i < n_vectors; i++) {
            cumulative += distances[i];
            if (cumulative >= r) {
                selected = i;
                break;
            }
        }

        memcpy(lvq->prototypes[p].vector, vectors + selected * lvq->dims, lvq->dims * sizeof(float));
        lvq->prototypes[p].label = labels[selected];
        used[selected] = 1;
    }

    /* 确保每个原型有不同的标签（尽量均匀分布） */
    /* 简化实现：重新分配标签 */
    for (int p = 0; p < lvq->n_prototypes; p++) {
        lvq->prototypes[p].label = p % n_labels;
    }

    free(distances);
    free(used);
}

/* ── 公共 API 实现 ── */

lvq_t *lvq_create(int dims, int n_prototypes)
{
    if (dims <= 0 || n_prototypes <= 0) {
        return NULL;
    }

    lvq_t *lvq = (lvq_t *)calloc(1, sizeof(lvq_t));
    if (!lvq) return NULL;

    lvq->dims = dims;
    lvq->n_prototypes = n_prototypes;
    lvq->code_size = 4;  /* 4 字节存索引 */
    lvq->alpha = LVQ_DEFAULT_ALPHA;
    lvq->trained = 0;

    /* 分配原型 */
    lvq->prototypes = (lvq_prototype_t *)calloc(n_prototypes, sizeof(lvq_prototype_t));
    if (!lvq->prototypes) {
        free(lvq);
        return NULL;
    }

    for (int i = 0; i < n_prototypes; i++) {
        lvq->prototypes[i].vector = (float *)calloc(dims, sizeof(float));
        if (!lvq->prototypes[i].vector) {
            for (int j = 0; j < i; j++) {
                free(lvq->prototypes[j].vector);
            }
            free(lvq->prototypes);
            free(lvq);
            return NULL;
        }
        lvq->prototypes[i].label = -1;
    }

    return lvq;
}

int lvq_train(lvq_t *lvq, int n, const float *vectors, const int *labels)
{
    if (!lvq || n <= 0 || !vectors || !labels) {
        return -1;
    }

    /* 计算唯一标签数 */
    int max_label = 0;
    for (int i = 0; i < n; i++) {
        if (labels[i] > max_label) max_label = labels[i];
    }
    int n_labels = max_label + 1;

    srand((unsigned int)time(NULL));

    /* 初始化原型 */
    init_prototypes(lvq, vectors, n, labels, n_labels);

    /* 训练迭代 */
    float alpha = lvq->alpha;

    for (int iter = 0; iter < LVQ_MAX_ITERS; iter++) {
        int updates = 0;

        /* 随机顺序遍历训练数据 */
        for (int i = 0; i < n; i++) {
            /* 找到最近的原型 */
            int best_idx = 0;
            float best_dist = compute_l2_distance(vectors + i * lvq->dims,
                                                  lvq->prototypes[0].vector, lvq->dims);

            for (int p = 1; p < lvq->n_prototypes; p++) {
                float dist = compute_l2_distance(vectors + i * lvq->dims,
                                                lvq->prototypes[p].vector, lvq->dims);
                if (dist < best_dist) {
                    best_dist = dist;
                    best_idx = p;
                }
            }

            /* 学习规则 */
            if (labels[i] == lvq->prototypes[best_idx].label) {
                /* 标签相同：拉近 */
                for (int d = 0; d < lvq->dims; d++) {
                    lvq->prototypes[best_idx].vector[d] +=
                        alpha * (vectors[i * lvq->dims + d] - lvq->prototypes[best_idx].vector[d]);
                }
            } else {
                /* 标签不同：推远 */
                for (int d = 0; d < lvq->dims; d++) {
                    lvq->prototypes[best_idx].vector[d] -=
                        alpha * (vectors[i * lvq->dims + d] - lvq->prototypes[best_idx].vector[d]);
                }
            }

            updates++;
        }

        /* 学习率衰减 */
        alpha *= LVQ_ALPHA_DECAY;

        /* 提前停止 */
        if (updates == 0 || alpha < 1e-5f) {
            break;
        }
    }

    lvq->trained = 1;
    return 0;
}

int lvq_encode(const lvq_t *lvq, const float *vector)
{
    if (!lvq || !lvq->trained || !vector) {
        return -1;
    }

    int best_idx = 0;
    float best_dist = compute_l2_distance(vector, lvq->prototypes[0].vector, lvq->dims);

    for (int i = 1; i < lvq->n_prototypes; i++) {
        float dist = compute_l2_distance(vector, lvq->prototypes[i].vector, lvq->dims);
        if (dist < best_dist) {
            best_dist = dist;
            best_idx = i;
        }
    }

    return best_idx;
}

int lvq_encode_batch(const lvq_t *lvq, int n, const float *vectors, int *codes)
{
    if (!lvq || !lvq->trained || n <= 0 || !vectors || !codes) {
        return -1;
    }

    for (int i = 0; i < n; i++) {
        codes[i] = lvq_encode(lvq, vectors + i * lvq->dims);
    }

    return 0;
}

void lvq_get_prototype(const lvq_t *lvq, int idx, float *out_vector)
{
    if (!lvq || idx < 0 || idx >= lvq->n_prototypes || !out_vector) {
        return;
    }

    memcpy(out_vector, lvq->prototypes[idx].vector, lvq->dims * sizeof(float));
}

int lvq_get_label(const lvq_t *lvq, int idx)
{
    if (!lvq || idx < 0 || idx >= lvq->n_prototypes) {
        return -1;
    }

    return lvq->prototypes[idx].label;
}

void lvq_get_info(const lvq_t *lvq, int *out_dims, int *out_n_prototypes, int *out_code_size)
{
    if (!lvq) return;

    if (out_dims) *out_dims = lvq->dims;
    if (out_n_prototypes) *out_n_prototypes = lvq->n_prototypes;
    if (out_code_size) *out_code_size = lvq->code_size;
}

int lvq_save(const lvq_t *lvq, const char *path)
{
    if (!lvq || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 写入头部 */
    fwrite(&lvq->dims, sizeof(int), 1, fp);
    fwrite(&lvq->n_prototypes, sizeof(int), 1, fp);
    fwrite(&lvq->trained, sizeof(int), 1, fp);

    /* 写入原型 */
    for (int i = 0; i < lvq->n_prototypes; i++) {
        fwrite(lvq->prototypes[i].vector, sizeof(float), lvq->dims, fp);
        fwrite(&lvq->prototypes[i].label, sizeof(int), 1, fp);
    }

    fclose(fp);
    return 0;
}

lvq_t *lvq_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读取头部 */
    int dims, n_prototypes, trained;
    if (fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&n_prototypes, sizeof(int), 1, fp) != 1 ||
        fread(&trained, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 创建量化器 */
    lvq_t *lvq = lvq_create(dims, n_prototypes);
    if (!lvq) {
        fclose(fp);
        return NULL;
    }

    lvq->trained = trained;

    /* 读取原型 */
    for (int i = 0; i < n_prototypes; i++) {
        if (fread(lvq->prototypes[i].vector, sizeof(float), dims, fp) != (size_t)dims) {
            fclose(fp);
            lvq_destroy(lvq);
            return NULL;
        }
        if (fread(&lvq->prototypes[i].label, sizeof(int), 1, fp) != 1) {
            fclose(fp);
            lvq_destroy(lvq);
            return NULL;
        }
    }

    fclose(fp);
    return lvq;
}

void lvq_destroy(lvq_t *lvq)
{
    if (!lvq) return;

    if (lvq->prototypes) {
        for (int i = 0; i < lvq->n_prototypes; i++) {
            free(lvq->prototypes[i].vector);
        }
        free(lvq->prototypes);
    }

    free(lvq);
}