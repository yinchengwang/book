#include "dict_private.h"

/* FNV-1a 64-bit 哈�? 与 BM25 使用相同算法 */
uint64_t dict_hash_fnv1a(const char *value)
{
    uint64_t hash = 14695981039346656037ULL;
    const unsigned char *bytes = (const unsigned char *)value;

    if (!value) {
        return 0;
    }
    while (*bytes) {
        hash ^= (uint64_t)(*bytes);
        hash *= 1099511628211ULL;
        bytes += 1;
    }
    return hash;
}

/* ── 停用词哈希集合 ─────────────────────────────── */

#define DICT_HASH_INITIAL_BUCKETS 64
#define DICT_HASH_LOAD_FACTOR 2.0

void dict_hash_set_init(dict_hash_set_t *set)
{
    if (!set) return;
    set->buckets = (dict_hash_bucket_t **)calloc(DICT_HASH_INITIAL_BUCKETS, sizeof(dict_hash_bucket_t *));
    set->bucket_count = set->buckets ? DICT_HASH_INITIAL_BUCKETS : 0;
    set->item_count = 0;
}

void dict_hash_set_destroy(dict_hash_set_t *set)
{
    int32_t i;

    if (!set || !set->buckets) return;
    for (i = 0; i < set->bucket_count; ++i) {
        dict_hash_bucket_t *b = set->buckets[i];
        while (b) {
            dict_hash_bucket_t *next = b->next;
            free(b->word);
            free(b);
            b = next;
        }
    }
    free(set->buckets);
    set->buckets = NULL;
    set->bucket_count = 0;
    set->item_count = 0;
}

static int dict_hash_set_resize(dict_hash_set_t *set, int32_t new_bucket_count)
{
    dict_hash_bucket_t **new_buckets;
    int32_t i;

    new_buckets = (dict_hash_bucket_t **)calloc((size_t)new_bucket_count, sizeof(dict_hash_bucket_t *));
    if (!new_buckets) return -1;
    for (i = 0; i < set->bucket_count; ++i) {
        dict_hash_bucket_t *b = set->buckets[i];
        while (b) {
            dict_hash_bucket_t *next = b->next;
            int32_t idx = (int32_t)(b->hash_value % (uint64_t)new_bucket_count);
            b->next = new_buckets[idx];
            new_buckets[idx] = b;
            b = next;
        }
    }
    free(set->buckets);
    set->buckets = new_buckets;
    set->bucket_count = new_bucket_count;
    return 0;
}

int dict_hash_set_add(dict_hash_set_t *set, const char *word)
{
    uint64_t hash;
    int32_t idx;
    dict_hash_bucket_t *b;

    if (!set || !set->buckets || !word) return -1;
    hash = dict_hash_fnv1a(word);
    idx = (int32_t)(hash % (uint64_t)set->bucket_count);
    for (b = set->buckets[idx]; b; b = b->next) {
        if (b->hash_value == hash && strcmp(b->word, word) == 0) return 0;
    }
    if ((double)set->item_count / (double)set->bucket_count >= DICT_HASH_LOAD_FACTOR) {
        if (dict_hash_set_resize(set, set->bucket_count * 2) != 0) return -1;
        idx = (int32_t)(hash % (uint64_t)set->bucket_count);
    }
    b = (dict_hash_bucket_t *)calloc(1, sizeof(dict_hash_bucket_t));
    if (!b) return -1;
    b->word = dict_strdup(word);
    if (!b->word) { free(b); return -1; }
    b->hash_value = hash;
    b->next = set->buckets[idx];
    set->buckets[idx] = b;
    set->item_count += 1;
    return 0;
}

bool dict_hash_set_contains(const dict_hash_set_t *set, const char *word)
{
    uint64_t hash;
    int32_t idx;
    dict_hash_bucket_t *b;

    if (!set || !set->buckets || !word) return false;
    hash = dict_hash_fnv1a(word);
    idx = (int32_t)(hash % (uint64_t)set->bucket_count);
    for (b = set->buckets[idx]; b; b = b->next) {
        if (b->hash_value == hash && strcmp(b->word, word) == 0) return true;
    }
    return false;
}

/* ── 同义词哈希映射 ─────────────────────────────── */

void dict_hash_map_init(dict_hash_map_t *map)
{
    if (!map) return;
    map->buckets = (dict_hash_map_bucket_t **)calloc(DICT_HASH_INITIAL_BUCKETS, sizeof(dict_hash_map_bucket_t *));
    map->bucket_count = map->buckets ? DICT_HASH_INITIAL_BUCKETS : 0;
    map->item_count = 0;
}

void dict_hash_map_destroy(dict_hash_map_t *map)
{
    int32_t i;

    if (!map || !map->buckets) return;
    for (i = 0; i < map->bucket_count; ++i) {
        dict_hash_map_bucket_t *b = map->buckets[i];
        while (b) {
            dict_hash_map_bucket_t *next = b->next;
            free(b->word);
            free(b->canonical_word);
            free(b);
            b = next;
        }
    }
    free(map->buckets);
    map->buckets = NULL;
    map->bucket_count = 0;
    map->item_count = 0;
}

static int dict_hash_map_resize(dict_hash_map_t *map, int32_t new_bucket_count)
{
    dict_hash_map_bucket_t **new_buckets;
    int32_t i;

    new_buckets = (dict_hash_map_bucket_t **)calloc((size_t)new_bucket_count, sizeof(dict_hash_map_bucket_t *));
    if (!new_buckets) return -1;
    for (i = 0; i < map->bucket_count; ++i) {
        dict_hash_map_bucket_t *b = map->buckets[i];
        while (b) {
            dict_hash_map_bucket_t *next = b->next;
            int32_t idx = (int32_t)(b->hash_value % (uint64_t)new_bucket_count);
            b->next = new_buckets[idx];
            new_buckets[idx] = b;
            b = next;
        }
    }
    free(map->buckets);
    map->buckets = new_buckets;
    map->bucket_count = new_bucket_count;
    return 0;
}

int dict_hash_map_put(dict_hash_map_t *map, const char *word, const char *canonical_word)
{
    uint64_t hash;
    int32_t idx;
    dict_hash_map_bucket_t *b;

    if (!map || !map->buckets || !word || !canonical_word) return -1;
    hash = dict_hash_fnv1a(word);
    idx = (int32_t)(hash % (uint64_t)map->bucket_count);
    for (b = map->buckets[idx]; b; b = b->next) {
        if (b->hash_value == hash && strcmp(b->word, word) == 0) {
            free(b->canonical_word);
            b->canonical_word = dict_strdup(canonical_word);
            return b->canonical_word ? 0 : -1;
        }
    }
    if ((double)map->item_count / (double)map->bucket_count >= DICT_HASH_LOAD_FACTOR) {
        if (dict_hash_map_resize(map, map->bucket_count * 2) != 0) return -1;
        idx = (int32_t)(hash % (uint64_t)map->bucket_count);
    }
    b = (dict_hash_map_bucket_t *)calloc(1, sizeof(dict_hash_map_bucket_t));
    if (!b) return -1;
    b->word = dict_strdup(word);
    b->canonical_word = dict_strdup(canonical_word);
    if (!b->word || !b->canonical_word) {
        free(b->word); free(b->canonical_word); free(b);
        return -1;
    }
    b->hash_value = hash;
    b->next = map->buckets[idx];
    map->buckets[idx] = b;
    map->item_count += 1;
    return 0;
}

const char *dict_hash_map_get(const dict_hash_map_t *map, const char *word)
{
    uint64_t hash;
    int32_t idx;
    dict_hash_map_bucket_t *b;

    if (!map || !map->buckets || !word) return NULL;
    hash = dict_hash_fnv1a(word);
    idx = (int32_t)(hash % (uint64_t)map->bucket_count);
    for (b = map->buckets[idx]; b; b = b->next) {
        if (b->hash_value == hash && strcmp(b->word, word) == 0) return b->canonical_word;
    }
    return NULL;
}

static void dict_free_word_list(dict_word_entry_t *head)
{
    dict_word_entry_t *entry = head;

    while (entry) {
        dict_word_entry_t *next = entry->next;

        free(entry->word);
        free(entry);
        entry = next;
    }
}

dict_node_t *dict_node_create(void)
{
    /* trie 节点按需创建，初�?children 为空哈希表�?*/
    return (dict_node_t *)calloc(1, sizeof(dict_node_t));
}

void dict_node_destroy(dict_node_t *node)
{
    dict_edge_t *edge;
    dict_edge_t *tmp;

    if (!node) {
        return;
    }

    /* 递归释放整棵 trie，通过 uthash 遍历（root_children 只是索引缓存，不拥有边） */
    HASH_ITER(hh, node->children, edge, tmp)
    {
        HASH_DEL(node->children, edge);
        dict_node_destroy(edge->child);
        free(edge);
    }
    free(node->root_children);
    free(node);
}

dict_edge_t *dict_find_edge(const dict_node_t *node, uint32_t codepoint)
{
    if (!node) {
        return NULL;
    }

    /* root 节点直接数组索引，O(1) 无需哈希计算 */
    if (node->root_children && codepoint < DICT_TRIE_ROOT_SIZE) {
        return node->root_children[codepoint];
    }

    dict_edge_t *edge = NULL;
    HASH_FIND(hh, node->children, &codepoint, sizeof(codepoint), edge);
    return edge;
}

dict_edge_t *dict_get_or_create_edge(dict_node_t *node, uint32_t codepoint)
{
    dict_edge_t *edge;

    if (!node) {
        return NULL;
    }

    /* root 节点数组快速路径：先查缓存，未命中则创建并�?��入 uthash + 缓存 */
    if (node->root_children && codepoint < DICT_TRIE_ROOT_SIZE) {
        edge = node->root_children[codepoint];
        if (edge) {
            return edge;
        }
        edge = (dict_edge_t *)calloc(1, sizeof(dict_edge_t));
        if (!edge) {
            return NULL;
        }
        edge->child = dict_node_create();
        if (!edge->child) {
            free(edge);
            return NULL;
        }
        edge->codepoint = codepoint;
        HASH_ADD(hh, node->children, codepoint, sizeof(edge->codepoint), edge);
        node->root_children[codepoint] = edge;
        return edge;
    }

    HASH_FIND(hh, node->children, &codepoint, sizeof(codepoint), edge);
    if (edge) {
        return edge;
    }

    /* 当前 codepoint 对应的边不存在时，连同子节点一起创建�?*/
    edge = (dict_edge_t *)calloc(1, sizeof(dict_edge_t));
    if (!edge) {
        return NULL;
    }
    edge->child = dict_node_create();
    if (!edge->child) {
        free(edge);
        return NULL;
    }
    edge->codepoint = codepoint;
    HASH_ADD(hh, node->children, codepoint, sizeof(edge->codepoint), edge);
    return edge;
}

char *dict_strdup_slice(const char *text, int32_t byte_start, int32_t byte_length)
{
    char *copy;

    if (!text || byte_start < 0 || byte_length < 0) {
        return NULL;
    }

    copy = (char *)malloc((size_t)byte_length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, text + byte_start, (size_t)byte_length);
    copy[byte_length] = '\0';
    return copy;
}

char *dict_strdup(const char *text)
{
    size_t length;
    char *copy;

    if (!text) {
        return NULL;
    }
    length = strlen(text);
    copy = (char *)malloc(length + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, length + 1);
    return copy;
}

int dict_utf8_decode_one(const char *text, int32_t remaining, uint32_t *codepoint, int32_t *byte_length)
{
    const unsigned char *bytes = (const unsigned char *)text;

    if (!text || remaining <= 0 || !codepoint || !byte_length) {
        return -1;
    }

    if (bytes[0] < 0x80) {
        *codepoint = bytes[0];
        *byte_length = 1;
        return 0;
    }
    if ((bytes[0] & 0xE0) == 0xC0 && remaining >= 2 && (bytes[1] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(bytes[0] & 0x1F) << 6) | (uint32_t)(bytes[1] & 0x3F);
        *byte_length = 2;
        return 0;
    }
    if ((bytes[0] & 0xF0) == 0xE0 && remaining >= 3 && (bytes[1] & 0xC0) == 0x80 && (bytes[2] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(bytes[0] & 0x0F) << 12) |
                     ((uint32_t)(bytes[1] & 0x3F) << 6) |
                     (uint32_t)(bytes[2] & 0x3F);
        *byte_length = 3;
        return 0;
    }
    if ((bytes[0] & 0xF8) == 0xF0 && remaining >= 4 && (bytes[1] & 0xC0) == 0x80 &&
        (bytes[2] & 0xC0) == 0x80 && (bytes[3] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(bytes[0] & 0x07) << 18) |
                     ((uint32_t)(bytes[1] & 0x3F) << 12) |
                     ((uint32_t)(bytes[2] & 0x3F) << 6) |
                     (uint32_t)(bytes[3] & 0x3F);
        *byte_length = 4;
        return 0;
    }

    *codepoint = bytes[0];
    *byte_length = 1;
    return 0;
}

bool dict_is_cjk(uint32_t codepoint)
{
    return (codepoint >= 0x4E00u && codepoint <= 0x9FFFu) ||
           (codepoint >= 0x3400u && codepoint <= 0x4DBFu) ||
           (codepoint >= 0xF900u && codepoint <= 0xFAFFu);
}

bool dict_is_ascii_word_char(uint32_t codepoint)
{
    return codepoint < 128u && (isalnum((int)codepoint) || codepoint == '_');
}

bool dict_is_delimiter(uint32_t codepoint)
{
    static const uint32_t k_unicode_delimiters[] = {
        0x3001u, 0x3002u, 0xFF0Cu, 0xFF1Bu, 0xFF1Au, 0xFF01u, 0xFF1Fu,
        0x300Au, 0x300Bu, 0x201Cu, 0x201Du, 0x2018u, 0x2019u,
        0xFF08u, 0xFF09u, 0x3010u, 0x3011u,
    };
    size_t i;

    if (codepoint < 128u) {
        return isspace((int)codepoint) || ispunct((int)codepoint);
    }

    for (i = 0; i < sizeof(k_unicode_delimiters) / sizeof(k_unicode_delimiters[0]); ++i) {
        if (k_unicode_delimiters[i] == codepoint) {
            return true;
        }
    }
    return false;
}

int dict_append_token(token_t **tokens,
                           int32_t *token_count,
                           int32_t *token_capacity,
                           const char *text,
                           int32_t byte_start,
                           int32_t byte_length,
                           bool is_ascii)
{
    token_t *new_tokens;
    char *token_text;
    int32_t new_capacity;

    if (!tokens || !token_count || !token_capacity || !text || byte_start < 0 || byte_length <= 0) {
        return -1;
    }

    /* token 数组按倍增扩容�?*/
    if (*token_count >= *token_capacity) {
        new_capacity = *token_capacity > 0 ? *token_capacity * 2 : 8;
        new_tokens = (token_t *)realloc(*tokens, (size_t)new_capacity * sizeof(token_t));
        if (!new_tokens) {
            return -1;
        }
        *tokens = new_tokens;
        *token_capacity = new_capacity;
    }

    /* token 文本做独立拷贝，避免依赖原始输入缓冲区生命周期�?*/
    token_text = dict_strdup_slice(text, byte_start, byte_length);
    if (!token_text) {
        return -1;
    }

    (*tokens)[*token_count].text = token_text;
    (*tokens)[*token_count].byte_start = byte_start;
    (*tokens)[*token_count].byte_length = byte_length;
    (*tokens)[*token_count].is_ascii = is_ascii;
    *token_count += 1;
    return 0;
}

int32_t dict_add_stop_word(dict_t *dict, const char *word)
{
    if (!dict || !word || word[0] == '\0') {
        return -1;
    }
    return dict_hash_set_add(&dict->stop_words, word);
}

int32_t dict_add_synonym(dict_t *dict, const char *word, const char *canonical_word)
{
    if (!dict || !word || word[0] == '\0' || !canonical_word || canonical_word[0] == '\0') {
        return -1;
    }
    return dict_hash_map_put(&dict->synonyms, word, canonical_word);
}

bool dict_is_stop_word(const dict_t *dict, const char *word)
{
    if (!dict || !word || word[0] == '\0') {
        return false;
    }
    return dict_hash_set_contains(&dict->stop_words, word);
}

const char *dict_resolve_synonym(const dict_t *dict, const char *word)
{
    const char *canonical;

    if (!dict || !word || word[0] == '\0') {
        return NULL;
    }
    canonical = dict_hash_map_get(&dict->synonyms, word);
    return canonical ? canonical : word;
}

int32_t dict_normalize_term(const dict_t *dict, const char *term, char **normalized_term)
{
    const char *effective_term;

    if (!dict || !normalized_term) {
        return -1;
    }

    *normalized_term = NULL;
    if (!term || term[0] == '\0') {
        return 0;
    }
    if (dict_is_stop_word(dict, term)) {
        return 0;
    }

    effective_term = dict_resolve_synonym(dict, term);
    if (!effective_term || effective_term[0] == '\0' || dict_is_stop_word(dict, effective_term)) {
        return 0;
    }

    *normalized_term = dict_strdup(effective_term);
    return *normalized_term ? 0 : -1;
}

int dict_append_normalized_token(const dict_t *dict,
                                      token_t **tokens,
                                      int32_t *token_count,
                                      int32_t *token_capacity,
                                      const char *text,
                                      int32_t byte_start,
                                      int32_t byte_length,
                                      bool is_ascii)
{
    char *raw_text;
    char *normalized_text;
    token_t *new_tokens;
    int32_t new_capacity;

    if (!dict || !tokens || !token_count || !token_capacity || !text || byte_start < 0 || byte_length <= 0) {
        return -1;
    }

    raw_text = dict_strdup_slice(text, byte_start, byte_length);
    if (!raw_text) {
        return -1;
    }

    normalized_text = NULL;
    if (dict_normalize_term(dict, raw_text, &normalized_text) != 0) {
        free(raw_text);
        return -1;
    }
    free(raw_text);

    if (!normalized_text) {
        return 0;
    }

    if (*token_count >= *token_capacity) {
        new_capacity = *token_capacity > 0 ? *token_capacity * 2 : 8;
        new_tokens = (token_t *)realloc(*tokens, (size_t)new_capacity * sizeof(token_t));
        if (!new_tokens) {
            free(normalized_text);
            return -1;
        }
        *tokens = new_tokens;
        *token_capacity = new_capacity;
    }

    (*tokens)[*token_count].text = normalized_text;
    (*tokens)[*token_count].byte_start = byte_start;
    (*tokens)[*token_count].byte_length = byte_length;
    (*tokens)[*token_count].is_ascii = is_ascii;
    *token_count += 1;
    return 0;
}

dict_t *dict_create(void)
{
    dict_t *dict = (dict_t *)calloc(1, sizeof(dict_t));

    if (!dict) {
        return NULL;
    }
    dict->root = dict_node_create();
    if (!dict->root) {
        free(dict);
        return NULL;
    }
    /* root 节点分配 codepoint → edge 直接索引缓存 */
    dict->root->root_children = (dict_edge_t **)calloc(DICT_TRIE_ROOT_SIZE, sizeof(dict_edge_t *));
    if (!dict->root->root_children) {
        dict_node_destroy(dict->root);
        free(dict);
        return NULL;
    }
    dict_hash_set_init(&dict->stop_words);
    dict_hash_map_init(&dict->synonyms);
    dict->hmm_enabled = true;
    dict_hmm_model_init_default(&dict->hmm_model);
    return dict;
}

dict_t *dict_create_default(void)
{
    dict_t *dict = dict_create();

    if (!dict) {
        return NULL;
    }
    if (dict_load_default(dict) != 0) {
        dict_drop(dict);
        return NULL;
    }
    return dict;
}

int32_t dict_add_word(dict_t *dict, const char *word, int32_t freq)
{
    dict_node_t *node;
    dict_word_entry_t *entry;
    int word_exists;
    int32_t offset;
    int32_t length;
    int32_t word_length;
    uint32_t codepoint;

    if (!dict || !dict->root || !word || word[0] == '\0' || freq <= 0) {
        return -1;
    }

    node = dict->root;
    word_length = (int32_t)strlen(word);
    for (offset = 0; offset < word_length; offset += length) {
        dict_edge_t *edge;

        if (dict_utf8_decode_one(word + offset, word_length - offset, &codepoint, &length) != 0 || length <= 0) {
            return -1;
        }
        edge = dict_get_or_create_edge(node, codepoint);
        if (!edge) {
            return -1;
        }
        node = edge->child;
    }

    word_exists = node->is_word;
    if (!word_exists) {
        entry = (dict_word_entry_t *)calloc(1, sizeof(dict_word_entry_t));
        if (!entry) {
            return -1;
        }
        entry->word = dict_strdup(word);
        if (!entry->word) {
            free(entry);
            return -1;
        }
        entry->next = dict->words;
        dict->words = entry;
        dict->word_count += 1;
        dict->total_freq += freq;
    } else {
        dict->total_freq += (int64_t)freq - (int64_t)node->freq;
    }

    node->is_word = true;
    node->freq = freq;
    return 0;
}

int32_t dict_load_default(dict_t *dict)
{
    if (!dict) {
        return -1;
    }

#ifdef DICT_DEFAULT_FILE_PATH
    if (dict_load_jieba_file(dict, DICT_DEFAULT_FILE_PATH) == 0) {
        return 0;
    }
#endif

    return dict_load_jieba_buffer(dict, dict_embedded_jieba_dict());
}

int32_t dict_size(const dict_t *dict)
{
    if (!dict) {
        return -1;
    }
    return dict->word_count;
}

void dict_reset(dict_t *dict)
{
    if (!dict) {
        return;
    }
    dict_node_destroy(dict->root);
    dict_free_word_list(dict->words);
    dict_hash_set_destroy(&dict->stop_words);
    dict_hash_map_destroy(&dict->synonyms);
    dict->root = dict_node_create();
    dict->word_count = 0;
    dict->total_freq = 0;
    dict->words = NULL;
    if (dict->root) {
        dict->root->root_children = (dict_edge_t **)calloc(DICT_TRIE_ROOT_SIZE, sizeof(dict_edge_t *));
    }
    dict_hash_set_init(&dict->stop_words);
    dict_hash_map_init(&dict->synonyms);
}

void dict_free_tokens(token_t *tokens, int32_t token_count)
{
    int32_t i;

    if (!tokens) {
        return;
    }
    for (i = 0; i < token_count; ++i) {
        free(tokens[i].text);
    }
    free(tokens);
}

int32_t dict_enable_hmm(dict_t *dict, bool enabled)
{
    if (!dict) return -1;
    dict->hmm_enabled = enabled;
    return 0;
}

int32_t dict_set_hmm_model(dict_t *dict, const dict_hmm_model_t *model)
{
    if (!dict) return -1;
    if (model) {
        dict->hmm_model = *model;
    } else {
        dict_hmm_model_init_default(&dict->hmm_model);
    }
    return 0;
}

void dict_drop(dict_t *dict)
{
    if (!dict) {
        return;
    }
    dict_node_destroy(dict->root);
    dict_free_word_list(dict->words);
    dict_hash_set_destroy(&dict->stop_words);
    dict_hash_map_destroy(&dict->synonyms);
    free(dict);
}