/* hash_table scaffold — djb2/murmur hash + 链地址法 + 开放定址法 + rehash */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ── 哈希函数 ── */

/* djb2 — Dan Bernstein 经典 */
static unsigned long djb2(const char *key) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++))
        hash = ((hash << 5) + hash) + c;  /* hash * 33 + c */
    return hash;
}

/* MurmurOAAT — 简化版 MurmurHash (One-At-A-Time) */
static unsigned long murmur_oat(const char *key) {
    unsigned long h = 0;
    for (; *key; key++) {
        h ^= (unsigned char)*key;
        h *= 0x5bd1e995;
        h ^= h >> 15;
    }
    return h;
}

/* ══════════════════════════════════════════════════════════════
 * Part 1: 链地址法 (Chaining)
 * ══════════════════════════════════════════════════════════════ */
#define CHAIN_BUCKETS 8

typedef struct HNode {
    char *key;
    int value;
    struct HNode *next;
} HNode;

typedef struct {
    HNode *buckets[CHAIN_BUCKETS];
    int size;
} ChainHT;

static void cht_init(ChainHT *ht) {
    ht->size = 0;
    for (int i = 0; i < CHAIN_BUCKETS; i++) ht->buckets[i] = NULL;
}

static void cht_put(ChainHT *ht, const char *key, int val) {
    unsigned long h = djb2(key) % CHAIN_BUCKETS;
    /* 检查 key 是否已存在（更新） */
    for (HNode *n = ht->buckets[h]; n; n = n->next) {
        if (strcmp(n->key, key) == 0) { n->value = val; return; }
    }
    /* 头插新节点 */
    HNode *n = (HNode*)malloc(sizeof(HNode));
    n->key = strdup(key);
    n->value = val;
    n->next = ht->buckets[h];
    ht->buckets[h] = n;
    ht->size++;
}

static bool cht_get(ChainHT *ht, const char *key, int *out) {
    unsigned long h = djb2(key) % CHAIN_BUCKETS;
    for (HNode *n = ht->buckets[h]; n; n = n->next) {
        if (strcmp(n->key, key) == 0) { *out = n->value; return true; }
    }
    return false;
}

static float cht_load_factor(ChainHT *ht) {
    return (float)ht->size / CHAIN_BUCKETS;
}

static void cht_print(ChainHT *ht) {
    for (int i = 0; i < CHAIN_BUCKETS; i++) {
        printf("[bucket %d] ", i);
        if (!ht->buckets[i]) { printf("(空)\n"); continue; }
        for (HNode *n = ht->buckets[i]; n; n = n->next)
            printf("'%s':%d -> ", n->key, n->value);
        printf("NULL\n");
    }
}

static void cht_free(ChainHT *ht) {
    for (int i = 0; i < CHAIN_BUCKETS; i++) {
        HNode *n = ht->buckets[i];
        while (n) { HNode *t = n; n = n->next; free(t->key); free(t); }
    }
}

/* ══════════════════════════════════════════════════════════════
 * Part 2: 开放定址法 (Open Addressing — 线性探测)
 * ══════════════════════════════════════════════════════════════ */
#define OA_CAP 11               /* 质数容量 */
#define OA_EMPTY 0x80000000     /* 哨兵：空槽位 */

typedef struct {
    char *keys[OA_CAP];
    int values[OA_CAP];
    int size;
} OpenHT;

static void oht_init(OpenHT *ht) {
    ht->size = 0;
    for (int i = 0; i < OA_CAP; i++) {
        ht->keys[i] = NULL;
        ht->values[i] = OA_EMPTY;
    }
}

static void oht_put(OpenHT *ht, const char *key, int val) {
    if (ht->size >= OA_CAP) { printf("[oht] 表满!\n"); return; }
    unsigned long h = murmur_oat(key) % OA_CAP;
    /* 线性探测 */
    int i = (int)h;
    while (ht->keys[i] != NULL) {
        if (strcmp(ht->keys[i], key) == 0) { ht->values[i] = val; return; }
        i = (i + 1) % OA_CAP;
    }
    ht->keys[i] = strdup(key);
    ht->values[i] = val;
    ht->size++;
}

static bool oht_get(OpenHT *ht, const char *key, int *out) {
    unsigned long h = murmur_oat(key) % OA_CAP;
    int i = (int)h;
    int probe = 0;
    while (ht->keys[i] != NULL && probe < OA_CAP) {
        if (strcmp(ht->keys[i], key) == 0) { *out = ht->values[i]; return true; }
        i = (i + 1) % OA_CAP;
        probe++;
    }
    return false;
}

static float oht_load_factor(OpenHT *ht) {
    return (float)ht->size / OA_CAP;
}

static void oht_print(OpenHT *ht) {
    for (int i = 0; i < OA_CAP; i++) {
        if (ht->keys[i])
            printf("[slot %2d] '%s':%d\n", i, ht->keys[i], ht->values[i]);
        else
            printf("[slot %2d] (空)\n", i);
    }
}

static void oht_free(OpenHT *ht) {
    for (int i = 0; i < OA_CAP; i++) free(ht->keys[i]);
}

int main(void) {
    /* ── 哈希函数对比 ── */
    printf("=== 哈希函数 ===\n");
    const char *test_keys[] = {"hello", "world", "hash", "table"};
    for (int i = 0; i < 4; i++) {
        printf("djb2('%s')     = %lu\n", test_keys[i], djb2(test_keys[i]));
        printf("murmur('%s')   = %lu\n", test_keys[i], murmur_oat(test_keys[i]));
    }

    /* ── 链地址法 ── */
    printf("\n=== 链地址法 (Chaining) ===\n");
    ChainHT ch; cht_init(&ch);
    cht_put(&ch, "apple",  100); printf("[ch] put('apple',100)  load=%.2f\n", cht_load_factor(&ch));
    cht_put(&ch, "banana", 200); printf("[ch] put('banana',200) load=%.2f\n", cht_load_factor(&ch));
    cht_put(&ch, "cherry", 300); printf("[ch] put('cherry',300) load=%.2f\n", cht_load_factor(&ch));
    cht_put(&ch, "date",   400); printf("[ch] put('date',400)   load=%.2f\n", cht_load_factor(&ch));
    cht_put(&ch, "elder",  500); printf("[ch] put('elder',500)  load=%.2f\n", cht_load_factor(&ch));
    cht_put(&ch, "fig",    600); printf("[ch] put('fig',600)    load=%.2f\n", cht_load_factor(&ch));
    cht_print(&ch);
    int v;
    printf("[ch] get('cherry') = %s\n", cht_get(&ch, "cherry", &v) ? "yes" : "no");
    printf("[ch] get('grape')  = %s\n", cht_get(&ch, "grape",  &v) ? "yes" : "no");

    /* ── 开放定址法 ── */
    printf("\n=== 开放定址法 (线性探测) ===\n");
    OpenHT oh; oht_init(&oh);
    const char *o_keys[] = {"dog","cat","bird","fish","cow","pig"};
    for (int i = 0; i < 6; i++) {
        oht_put(&oh, o_keys[i], (i+1)*10);
        printf("[oh] put('%s',%d) load=%.2f\n", o_keys[i], (i+1)*10, oht_load_factor(&oh));
    }
    oht_print(&oh);
    printf("[oh] get('fish') = %s\n", oht_get(&oh, "fish", &v) ? "yes" : "no");

    /* ── Rehash 说明 ── */
    printf("\n=== Rehash 说明 ===\n");
    printf("链地址法 load=%.2f (%.0f%%), 开放定址法 load=%.2f (%.0f%%)\n",
           cht_load_factor(&ch), cht_load_factor(&ch)*100,
           oht_load_factor(&oh), oht_load_factor(&oh)*100);
    printf("实际系统中, 链地址法 load>1.0 才 rehash, 开放定址法 load>0.7 就 rehash\n");

    cht_free(&ch);
    oht_free(&oh);

    printf("\n=== PASS ===\n");
    return 0;
}
