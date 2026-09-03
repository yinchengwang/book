/* valgrind scaffold — 故意三处内存问题，valgrind 捕获 */

/* 需 Linux + Valgrind (POSIX-only)。本机 MinGW 跑不通——见 README */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    /* 1) memory leak：忘了 free */
    int *leak = malloc(8 * sizeof(int));
    for (int i = 0; i < 8; i++) leak[i] = i;
    /* leak 永远不被 free -> 8 ints definitely lost */

    /* 2) use-after-free */
    char *uaf = malloc(16);
    strcpy(uaf, "hello");
    free(uaf);
    /* 这里读 uaf 是 use-after-free */
    fprintf(stderr, "[valgrind] uaf str = '%s'\n", uaf);

    /* 3) uninitialized memory */
    int uninit[16];
    int sum = 0;
    for (int i = 0; i < 16; i++) sum += uninit[i];
    fprintf(stderr, "[valgrind] uninit sum = %d (取决于栈残留)\n", sum);

    /* 4) conditional jump depends on uninit */
    if (uninit[0] > 100) {
        fprintf(stderr, "this branch may run depending on stack dirt\n");
    }
    return 0;
}
