/*
 * main.c
 *
 * 演示 B+Tree 实现细节：页内结构、分裂、合并、平衡维护。
 * 对照 MySQL InnoDB 的 B+Tree 页面设计。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* 页面大小（模拟 InnoDB 16KB 页） */
#define PAGE_SIZE 16
#define SLOT_SIZE 4

/* Page Directory 槽结构 */
typedef struct page_slot {
    uint16_t offset;    /* 槽指向的记录在页内的偏移 */
} page_slot_t;

/* B+Tree 页面结构 */
typedef struct btree_page {
    uint8_t level;              /* 页面层级，0=叶子 */
    uint16_t n_records;         /* 页内记录数 */
    uint16_t free_space;        /* 空闲空间起始偏移 */
    page_slot_t slots[PAGE_SIZE]; /* Page Directory 槽数组 */
    int records[PAGE_SIZE];     /* 模拟记录数据（键值） */
    struct btree_page *parent;  /* 父节点指针（便于回溯） */
    struct btree_page *next;    /* 叶子节点链表（范围扫描） */
    struct btree_page *children[PAGE_SIZE + 1]; /* 子节点指针 */
} btree_page_t;

/* B+Tree 索引结构 */
typedef struct btree_index {
    btree_page_t *root;
    uint32_t height;
    uint32_t min_keys;  /* 每页最少键数 = ceil(PAGE_SIZE/2) */
} btree_index_t;

/* 在页内通过 Page Directory 二分查找定位记录 */
int page_binary_search(btree_page_t *page, int key) {
    int left = 0, right = page->n_records - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        int slot_offset = page->slots[mid].offset;
        int record_key = page->records[slot_offset];
        if (record_key == key) {
            return mid;  /* 命中 */
        } else if (record_key < key) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return left;  /* 返回插入位置 */
}

/* 创建新页面 */
btree_page_t *page_create(uint8_t level) {
    btree_page_t *page = calloc(1, sizeof(btree_page_t));
    page->level = level;
    page->n_records = 0;
    page->free_space = 0;
    return page;
}

/* 页面分裂：满页分裂为两个半满页，中间键上浮到父节点 */
int page_split(btree_index_t *index, btree_page_t *page) {
    printf("  [分裂] 页面层级=%u, 记录数=%u -> 分裂\n", page->level, page->n_records);

    /* 创建新右页 */
    btree_page_t *right = page_create(page->level);
    uint32_t mid = page->n_records / 2;
    uint32_t i;

    /* 右半部分搬到新页 */
    for (i = mid + 1; i < page->n_records; i++) {
        right->records[right->n_records] = page->records[i];
        right->slots[right->n_records].offset = right->n_records;
        right->n_records++;
    }

    /* 原页保留左半部分 */
    page->n_records = mid;

    /* 处理子节点指针（非叶子节点） */
    if (!page->level == 0) {
        for (i = mid + 1; i <= page->n_records + right->n_records; i++) {
            right->children[i - mid - 1] = page->children[i];
            page->children[i] = NULL;
        }
    }

    /* 中间键上浮到父节点 */
    int mid_key = page->records[mid];
    if (page->parent == NULL) {
        /* 根节点分裂，创建新根 */
        btree_page_t *new_root = page_create(page->level + 1);
        new_root->records[0] = mid_key;
        new_root->slots[0].offset = 0;
        new_root->n_records = 1;
        new_root->children[0] = page;
        new_root->children[1] = right;
        page->parent = new_root;
        right->parent = new_root;
        index->root = new_root;
        index->height++;
        printf("  [分裂] 树高度增加: %u -> %u\n", index->height - 1, index->height);
    } else {
        /* 插入到父节点，可能触发递归分裂 */
        btree_page_t *parent = page->parent;
        int pos = page_binary_search(parent, mid_key);
        for (i = parent->n_records; i > pos; i--) {
            parent->records[i] = parent->records[i - 1];
            parent->slots[i] = parent->slots[i - 1];
        }
        parent->records[pos] = mid_key;
        parent->slots[pos].offset = pos;
        parent->n_records++;
        right->parent = parent;

        /* 更新父节点的子指针 */
        for (i = parent->n_records; i > pos + 1; i--) {
            parent->children[i] = parent->children[i - 1];
        }
        parent->children[pos + 1] = right;

        /* 父节点也可能满，需要递归分裂 */
        if (parent->n_records >= PAGE_SIZE) {
            page_split(index, parent);
        }
    }

    return 0;
}

/* 页面插入 */
int page_insert(btree_index_t *index, btree_page_t *page, int key) {
    int pos = page_binary_search(page, key);

    /* 检查是否已存在 */
    if (pos < page->n_records && page->records[page->slots[pos].offset] == key) {
        return 1;  /* 已存在 */
    }

    /* 页面已满，先分裂 */
    if (page->n_records >= PAGE_SIZE) {
        page_split(index, page);
        /* 分裂后需重新定位插入位置 */
        if (key > page->records[page->n_records - 1]) {
            page = page->parent->children[page->parent->n_records];
        }
        pos = page_binary_search(page, key);
    }

    /* 插入新记录 */
    for (int i = page->n_records; i > pos; i--) {
        page->records[i] = page->records[i - 1];
        page->slots[i] = page->slots[i - 1];
    }
    page->records[pos] = key;
    page->slots[pos].offset = pos;
    page->n_records++;

    return 0;
}

/* 向上回溯更新父节点键（删除后可能触发合并） */
void update_parent_keys(btree_index_t *index, btree_page_t *page) {
    while (page->parent != NULL) {
        btree_page_t *parent = page->parent;
        /* 找到当前页在父节点中的位置 */
        int child_pos = 0;
        for (int i = 0; i <= parent->n_records; i++) {
            if (parent->children[i] == page) {
                child_pos = i;
                break;
            }
        }
        /* 更新父节点中对应的分隔键 */
        if (child_pos > 0 && child_pos - 1 < parent->n_records) {
            parent->records[child_pos - 1] = page->records[0];
        }
        page = parent;
    }
}

/* 页面合并：删除后若低于最小键数，合并相邻页 */
int page_merge(btree_index_t *index, btree_page_t *page) {
    printf("  [合并] 页面层级=%u, 记录数=%u -> 尝试合并\n", page->level, page->n_records);

    if (page->parent == NULL) {
        /* 根节点 */
        if (page->n_records == 0 && page->children[0] != NULL) {
            /* 根节点空了，下降一层 */
            index->root = page->children[0];
            index->root->parent = NULL;
            index->height--;
            printf("  [合并] 树高度减少: %u -> %u\n", index->height + 1, index->height);
            free(page);
        }
        return 0;
    }

    btree_page_t *parent = page->parent;
    int child_pos = 0;
    for (int i = 0; i <= parent->n_records; i++) {
        if (parent->children[i] == page) {
            child_pos = i;
            break;
        }
    }

    /* 尝试与左兄弟或右兄弟合并 */
    btree_page_t *left_sib = (child_pos > 0) ? parent->children[child_pos - 1] : NULL;
    btree_page_t *right_sib = (child_pos < parent->n_records) ? parent->children[child_pos + 1] : NULL;

    /* 优先与左兄弟合并 */
    if (left_sib && left_sib->n_records + page->n_records < PAGE_SIZE) {
        /* 把当前页所有记录搬到左兄弟 */
        for (int i = 0; i < page->n_records; i++) {
            left_sib->records[left_sib->n_records + i] = page->records[i];
            left_sib->slots[left_sib->n_records + i] = page->slots[i];
        }
        left_sib->n_records += page->n_records;

        /* 从父节点删除分隔键 */
        for (int i = child_pos - 1; i < parent->n_records - 1; i++) {
            parent->records[i] = parent->records[i + 1];
            parent->slots[i] = parent->slots[i + 1];
        }
        for (int i = child_pos; i < parent->n_records; i++) {
            parent->children[i] = parent->children[i + 1];
        }
        parent->n_records--;
        parent->children[parent->n_records + 1] = NULL;

        free(page);

        /* 父节点可能也需要合并 */
        if (parent->n_records < index->min_keys) {
            page_merge(index, parent);
        }
        return 0;
    }

    /* 与右兄弟合并 */
    if (right_sib && page->n_records + right_sib->n_records < PAGE_SIZE) {
        for (int i = 0; i < right_sib->n_records; i++) {
            page->records[page->n_records + i] = right_sib->records[i];
            page->slots[page->n_records + i] = right_sib->slots[i];
        }
        page->n_records += right_sib->n_records;

        for (int i = child_pos; i < parent->n_records - 1; i++) {
            parent->records[i] = parent->records[i + 1];
            parent->slots[i] = parent->slots[i + 1];
        }
        for (int i = child_pos + 1; i < parent->n_records; i++) {
            parent->children[i] = parent->children[i + 1];
        }
        parent->n_records--;
        parent->children[parent->n_records + 1] = NULL;

        free(right_sib);

        if (parent->n_records < index->min_keys) {
            page_merge(index, parent);
        }
        return 0;
    }

    return -1;  /* 无法合并 */
}

/* 页面删除 */
int page_delete(btree_index_t *index, btree_page_t *page, int key) {
    int pos = page_binary_search(page, key);

    if (pos >= page->n_records || page->records[page->slots[pos].offset] != key) {
        return -1;  /* 不存在 */
    }

    /* 删除记录 */
    for (int i = pos; i < page->n_records - 1; i++) {
        page->records[i] = page->records[i + 1];
        page->slots[i] = page->slots[i + 1];
    }
    page->n_records--;

    /* 更新父节点键 */
    update_parent_keys(index, page);

    /* 检查是否需要合并 */
    if (page->n_records < index->min_keys) {
        page_merge(index, page);
    }

    return 0;
}

/* 打印树结构 */
void print_tree(btree_page_t *page, int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
    printf("L%u [", page->level);
    for (int i = 0; i < page->n_records; i++) {
        printf("%d", page->records[i]);
        if (i < page->n_records - 1) printf(", ");
    }
    printf("]\n");

    if (page->level > 0) {
        for (int i = 0; i <= page->n_records; i++) {
            if (page->children[i]) {
                print_tree(page->children[i], depth + 1);
            }
        }
    }
}

int main(void) {
    btree_index_t index = {0};
    index.root = page_create(0);
    index.height = 1;
    index.min_keys = PAGE_SIZE / 2;

    printf("=== B+Tree 分裂演示 ===\n");
    printf("连续插入触发分裂:\n");

    /* 连续插入触发分裂 */
    for (int i = 1; i <= 25; i++) {
        printf("插入 %d: ", i);
        page_insert(&index, index.root, i);
        printf("树高度=%u\n", index.height);
        if (i % 8 == 0) {
            print_tree(index.root, 0);
            printf("\n");
        }
    }

    printf("\n最终树结构:\n");
    print_tree(index.root, 0);

    printf("\n=== B+Tree 合并演示 ===\n");
    printf("连续删除触发合并:\n");

    /* 连续删除触发合并 */
    for (int i = 25; i >= 1; i--) {
        printf("删除 %d: ", i);
        page_delete(&index, index.root, i);
        printf("树高度=%u\n", index.height);
        if (i % 8 == 0) {
            print_tree(index.root, 0);
            printf("\n");
        }
    }

    printf("\n最终树结构:\n");
    print_tree(index.root, 0);

    return 0;
}