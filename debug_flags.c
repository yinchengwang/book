#include <stdio.h>
#include <stdint.h>

#define BTP_LEAF       0x01
#define BTP_ROOT       0x02
#define BTP_INTERNAL   0x04

#define BTP_LEAF_FLAG        0x0001
#define BTP_ROOT_FLAG        0x0002
#define BTP_INTERNAL_FLAG    0x0004

int main() {
    printf("BTP_LEAF=0x%x, BTP_LEAF_FLAG=0x%x, equal=%d\n", BTP_LEAF, BTP_LEAF_FLAG, BTP_LEAF == BTP_LEAF_FLAG);
    printf("BTP_ROOT=0x%x, BTP_ROOT_FLAG=0x%x, equal=%d\n", BTP_ROOT, BTP_ROOT_FLAG, BTP_ROOT == BTP_ROOT_FLAG);
    printf("BTP_INTERNAL=0x%x, BTP_INTERNAL_FLAG=0x%x, equal=%d\n", BTP_INTERNAL, BTP_INTERNAL_FLAG, BTP_INTERNAL == BTP_INTERNAL_FLAG);

    uint16_t flags = BTP_LEAF_FLAG | BTP_ROOT_FLAG;
    printf("flags=0x%x, IS_LEAF=%d\n", flags, (int)((flags & BTP_LEAF) != 0));
    return 0;
}