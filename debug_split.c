#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define BTP_LEAF       0x01
#define BTP_ROOT       0x02
#define BTP_INTERNAL   0x04
#define BTP_HALF_DEAD  0x08

#define BTP_LEAF_FLAG        0x0001
#define BTP_ROOT_FLAG        0x0002
#define BTP_INTERNAL_FLAG    0x0004

#define BT_PAGE_IS_LEAF(header)  (((header)->btpo_flags & BTP_LEAF) != 0)
#define BT_PAGE_IS_LEAF_FLAG(header)  (((header)->btpo_flags & BTP_LEAF_FLAG) != 0)
#define BT_PAGE_IS_ROOT(header)  (((header)->btpo_flags & BTP_ROOT) != 0)
#define BT_PAGE_IS_ROOT_FLAG(header)  (((header)->btpo_flags & BTP_ROOT_FLAG) != 0)

typedef struct BTPageHeaderData {
    uint16_t btpo_flags;
    uint16_t btpo_level;
    uint32_t btpo_prev;
    uint32_t btpo_next;
    uint32_t btpo_rightlink;
    uint32_t btpo_cycleid;
    uint32_t btpo_xact;
    uint16_t btpo_offset;
    uint16_t btpo_count;
} BTPageHeaderData;

int main() {
    BTPageHeaderData header;
    memset(&header, 0, sizeof(header));
    
    // 模拟 bt_page_init 设置 flags
    header.btpo_flags = BTP_LEAF_FLAG | BTP_ROOT_FLAG;
    printf("After bt_page_init: btpo_flags = 0x%x\n", header.btpo_flags);
    printf("BT_PAGE_IS_LEAF (using BTP_LEAF): %s\n", BT_PAGE_IS_LEAF(&header) ? "YES" : "NO");
    printf("BT_PAGE_IS_ROOT (using BTP_ROOT): %s\n", BT_PAGE_IS_ROOT(&header) ? "YES" : "NO");
    
    // 模拟 btree_split_leaf 检查
    if (!BT_PAGE_IS_LEAF(&header)) {
        printf("FAIL: BT_PAGE_IS_LEAF said NO!\n");
    } else {
        printf("PASS: BT_PAGE_IS_LEAF said YES\n");
    }
    
    // 测试 buf_new 返回的 blocknum 为 0 的问题
    printf("\n--- buf_new 返回 blocknum = 0 的问题 ---\n");
    printf("如果不调用 buf_hash_insert 更新 blocknum，则 buf_get_blocknum 返回 0\n");
    printf("btree_split_leaf 调用 buf_new 后，必须更新 blocknum 或通过别的机制获取\n");
    
    return 0;
}
