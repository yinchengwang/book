/* struct_union scaffold — struct layout + bit field + union + flexible array */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* 1) struct layout：含 padding 演示 */
struct point {
    char x;      /* offset 0 */
    /* 3 bytes padding */
    int  y;      /* offset 4 */
    char z;      /* offset 8 */
    /* 3 bytes padding */
}; /* sizeof = 12 */

struct __attribute__((packed)) packed_point {
    char x;      /* offset 0 */
    int  y;      /* offset 1 */
    char z;      /* offset 5 */
}; /* sizeof = 6 */

/* 2) bit field */
struct flags {
    unsigned int a : 1;
    unsigned int b : 3;
    unsigned int c : 4;
}; /* sizeof = 4 */

/* 3) union：节省内存，节省到所有成员中最大的 */
union number {
    int   i;
    float f;
    char  bytes[4];
};

/* 4) flexible array member */
struct packet {
    size_t len;
    char   data[];
};

int main(void) {
    struct point p = { .x = 1, .y = 0x22222222, .z = 3 };
    printf("[struct]   offset(x)=%zu offset(y)=%zu offset(z)=%zu sizeof=%zu\n",
           offsetof(struct point, x),
           offsetof(struct point, y),
           offsetof(struct point, z),
           sizeof(struct point));

    struct packed_point pp = { .x = 1, .y = 0x22222222, .z = 3 };
    printf("[packed]   sizeof=%zu\n", sizeof(struct packed_point));

    struct flags f = { .a = 1, .b = 5, .c = 0xA };
    printf("[bitfld]   a=%u b=%u c=%u  (sizeof=%zu)\n", f.a, f.b, f.c, sizeof(f));

    union number n;
    n.i = 0x40490FDB;       /* pi in IEEE-754 */
    printf("[union]    sizeof=%zu, pi as int=0x%08X, as float=%.6f\n",
           sizeof(union number), n.i, n.f);

    /* flexible array member */
    struct packet *pkt = malloc(sizeof(struct packet) + 16);
    if (!pkt) return 1;
    pkt->len = 16;
    memcpy(pkt->data, "hello-flex-array!", 16);
    printf("[flex]     len=%zu data='%.*s'\n", pkt->len, (int)pkt->len, pkt->data);
    free(pkt);

    return 0;
}
