/* c11_features scaffold — _Generic / designated initializer / static_assert / anonymous union in struct */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* 1) _Generic 类型分支选择（编译期） */
#define PRINT_TYPE(x) _Generic((x),                                  \
    int:    printf("int: %d\n", (x)),                               \
    float:  printf("float: %.2f\n", (x)),                           \
    double: printf("double: %.6f\n", (x)),                          \
    char *: printf("char *: %s\n", (x)),                            \
    default: printf("unknown type\n"))

/* 2) static_assert（C11 关键字，编译期断言） */
static_assert(sizeof(void *) == 8, "本 demo 仅支持 64 位平台");
static_assert(sizeof(int) == 4, "本 demo 假设 int 是 32 位");

/* 3) designated initializer（字段顺序自由） */
struct config {
    int   port;
    char *host;
    int   workers;
};

/* 4) anonymous union in struct（C11 标准允许，省标签） */
struct packet_v2 {
    enum { PKT_DATA, PKT_CTRL } kind;
    union {
        struct { int len; char *data; } data;
        struct { int code;            } ctrl;
    };  /* 匿名 union，直接 pkt.data.len 访问 */
};

int main(void) {
    /* _Generic */
    int    i = 42;
    float  f = 3.14f;
    double d = 2.71828;
    char  *s = "hello";
    (void)i;
    PRINT_TYPE((int)42);
    PRINT_TYPE(f);
    PRINT_TYPE(d);
    PRINT_TYPE(s);

    /* designated initializer */
    struct config c = { .workers = 4, .host = "127.0.0.1", .port = 8080 };
    printf("[init] host=%s port=%d workers=%d\n", c.host, c.port, c.workers);

    /* anonymous union */
    struct packet_v2 p1 = { .kind = PKT_DATA, .data = { .len = 5, .data = "hello" } };
    struct packet_v2 p2 = { .kind = PKT_CTRL, .ctrl = { .code = 200 } };
    printf("[anon-union] p1.kind=%d data.len=%d data='%.*s'\n",
           p1.kind, p1.data.len, p1.data.len, p1.data.data);
    printf("[anon-union] p2.kind=%d ctrl.code=%d\n", p2.kind, p2.ctrl.code);

    /* _Alignas / _Alignof（C11） */
    _Alignas(16) char buf[64];
    printf("[align]   _Alignof(buf) = %zu\n", _Alignof(buf));

    return 0;
}
