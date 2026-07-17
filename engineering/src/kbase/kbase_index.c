#include "kbase_index.h"
#include "infra/infra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "algo-prod/distance/distance.h"
#include "algo-prod/quantization/quantization.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"

/* HNSW 函数指针（通过 dlsym 或静态链接时解析）
 * 这里使用前向声明+函数指针，避免直接依赖 faiss_hnsw
 */
typedef void* (*hnsw_create_fn)(int, int, int, int, int);
typedef int32_t (*hnsw_add_fn)(void*, int32_t, const float*);
typedef void (*hnsw_drop_fn)(void*);

/* 递归扫描笔记目录 */
static void scan_dir(kbase_index_t* idx, const char* dir) {
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.' || e->d_name[0] == '_') continue;  /* 跳过隐藏文件 */
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scan_dir(idx, path);
        } else {
            size_t nlen = strlen(e->d_name);
            if (nlen > 3 && strcmp(e->d_name + nlen - 3, ".md") == 0) {
                /* 跳过 README.md 和 _index.md */
                if (strcmp(e->d_name, "README.md") == 0) continue;
                if (strcmp(e->d_name, "_index.md") == 0) continue;
                if (idx->num_notes >= idx->capacity) {
                    size_t new_cap = idx->capacity * 2;
                    kbase_note_t* tmp = (kbase_note_t*)realloc(idx->notes, new_cap * sizeof(kbase_note_t));
                    if (!tmp) {
                        /* realloc 失败：保留原指针 idx->notes, 标记内存错误 */
                        idx->scan_error = INFRA_ERR_MEMORY;
                        continue;
                    }
                    idx->notes = tmp;
                    idx->capacity = new_cap;
                }
                kbase_note_t* note = &idx->notes[idx->num_notes];
                memset(note, 0, sizeof(*note));
                note->path = strdup(path);
                /* 简单读取标题：取第一行非空作为标题 */
                FILE* fp = fopen(path, "r");
                if (fp) {
                    char line[1024];
                    /* 跳过 frontmatter */
                    if (fgets(line, sizeof(line), fp) && strncmp(line, "---", 3) == 0) {
                        while (fgets(line, sizeof(line), fp) && strncmp(line, "---", 3) != 0);
                    }
                    /* 找第一个 # 开头的行 */
                    while (fgets(line, sizeof(line), fp)) {
                        if (line[0] == '#' && line[1] == ' ') {
                            char* t = line + 2;
                            size_t tl = strlen(t);
                            while (tl > 0 && (t[tl-1] == '\n' || t[tl-1] == '\r')) t[--tl] = 0;
                            note->title = strdup(t);
                            break;
                        }
                    }
                    /* 读全文内容 */
                    fseek(fp, 0, SEEK_END);
                    long sz = ftell(fp);
                    fseek(fp, 0, SEEK_SET);
                    char* content = (char*)malloc((size_t)sz + 1);
                    if (!content) { fclose(fp); continue; }
                    size_t bytes_read = fread(content, 1, (size_t)sz, fp);
                    if (bytes_read != (size_t)sz) { free(content); fclose(fp); continue; }
                    content[sz] = 0;
                    fclose(fp);
                    note->content = content;
                    if (!note->title) note->title = strdup(e->d_name);
                    idx->num_notes++;
                }
            }
        }
    }
    closedir(d);
}

int kbase_index_scan(kbase_index_t* idx) {
    if (!idx || !idx->notes_dir) return -1;
    idx->scan_error = 0;
    scan_dir(idx, idx->notes_dir);
    if (idx->scan_error != 0) return -1;
    return idx->num_notes;
}

kbase_index_t* kbase_index_create(const char* notes_dir) {
    kbase_index_t* idx = (kbase_index_t*)calloc(1, sizeof(*idx));
    if (!idx) return NULL;
    idx->capacity = 16;
    idx->notes = (kbase_note_t*)calloc(idx->capacity, sizeof(kbase_note_t));
    idx->notes_dir = strdup(notes_dir);
    idx->dim = 384;
    return idx;
}

void kbase_index_destroy(kbase_index_t* idx) {
    if (!idx) return;
    for (int i = 0; i < idx->num_notes; i++) {
        free(idx->notes[i].path);
        free(idx->notes[i].title);
        free(idx->notes[i].content);
        free(idx->notes[i].embedding);
    }
    free(idx->notes);
    free(idx->notes_dir);
    if (idx->hnsw) faiss_hnsw_index_drop((faiss_hnsw_t*)idx->hnsw);
    free(idx);
}

infra_status_t kbase_index_build(kbase_index_t* idx, infra_model_t* model) {
    if (!idx || !model) return INFRA_ERR_PARAM;
    if (idx->num_notes == 0) return INFRA_OK;

    /* 对每个笔记生成 Embedding */
    for (int i = 0; i < idx->num_notes; i++) {
        kbase_note_t* note = &idx->notes[i];
        const char* texts[1] = { note->content };
        float* embs = NULL;
        int dim = 0;
        infra_status_t s = infra_embed(model, texts, 1, &embs, &dim);
        if (s != INFRA_OK) return s;
        note->embedding = embs;
        note->embedding_dim = dim;
        note->id = i;
    }

    /* 创建 HNSW 索引并添加所有向量 */
    faiss_hnsw_t* hnsw = faiss_hnsw_index_create(
        16,                        /* M: 每个节点的邻居数 */
        idx->dim,                  /* dims: 向量维度 */
        200,                       /* ef_construction: 构建时的搜索宽度 */
        DISTANCE_METRIC_L2_SQUARED,  /* metric: L2 距离 */
        QUANTIZATION_TYPE_NONE       /* 不量化 */
    );
    if (!hnsw) return INFRA_ERR_MEMORY;

    for (int i = 0; i < idx->num_notes; i++) {
        kbase_note_t* note = &idx->notes[i];
        if (note->embedding) {
            faiss_hnsw_index_add(hnsw, 1, note->embedding);
            note->id = i;  /* 向量在 HNSW 中的位置 */
        }
    }
    idx->hnsw = hnsw;
    return INFRA_OK;
}

infra_status_t kbase_index_save(kbase_index_t* idx, const char* path) {
    if (!idx || !path) return INFRA_ERR_PARAM;
    char meta_path[1024], vec_path[1024];
    snprintf(meta_path, sizeof(meta_path), "%s.index", path);
    snprintf(vec_path, sizeof(vec_path), "%s.vec", path);
    FILE* mf = fopen(meta_path, "w");
    if (!mf) return INFRA_ERR_LOAD;
    fprintf(mf, "{\"num_notes\":%d,\"dim\":%d}\n", idx->num_notes, idx->dim);
    for (int i = 0; i < idx->num_notes; i++) {
        kbase_note_t* n = &idx->notes[i];
        fprintf(mf, "%d\t%s\t%s\n", n->id, n->path, n->title ? n->title : "");
    }
    fclose(mf);
    FILE* vf = fopen(vec_path, "wb");
    if (!vf) return INFRA_ERR_LOAD;
    for (int i = 0; i < idx->num_notes; i++) {
        if (idx->notes[i].embedding) {
            fwrite(idx->notes[i].embedding, sizeof(float), (size_t)idx->dim, vf);
        }
    }
    fclose(vf);
    return INFRA_OK;
}

infra_status_t kbase_index_load(kbase_index_t* idx, const char* path) {
    if (!idx || !path) return INFRA_ERR_PARAM;
    /* 占位实现：Phase 2 完善 */
    (void)path;
    return INFRA_ERR_NOT_FOUND;
}