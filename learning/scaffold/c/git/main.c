/*
 * git scaffold — Git 版本控制演示
 *
 * 本文件的三个"版本"（注释标注）演示了典型的 Git 工作流：
 *   v1.0: 初始实现（简单 strlen）
 *   v1.1: 引入 bug（忘记 NULL 检查）
 *   v1.2: 修复 bug（添加空指针防护）
 *
 * 复现：
 *   gcc -Wall -Wextra -O2 -std=c11 -o git_demo main.c
 *   ./git_demo "hello world"
 *   ./git_demo            ← 触发空指针 → v1.2 已修复
 *
 * 然后用 Git 追溯版本演进：
 *   git log --oneline main.c
 *   git diff HEAD~2 HEAD~1 main.c   ← 看 v1.0→v1.1 引入了什么 bug
 *   git diff HEAD~1 HEAD main.c     ← 看 v1.1→v1.2 如何修复
 */

#include <stdio.h>
#include <string.h>

/*
 * 版本演化（通过注释展示，实际 Git 历史中通过 diff 看到）：
 *
 * --- v1.0 ---
 * int compute_score(const char *input) {
 *     return (int)strlen(input);
 * }
 *
 * --- v1.1 --- (引入 bug: 未检查 NULL)
 * int compute_score(const char *input) {
 *     return (int)strlen(input) * 2;
 * }
 *
 * --- v1.2 --- (修复: 添加空指针防护)
 */
int compute_score(const char *input) {
    return input ? (int)strlen(input) * 2 : 0;
}

int main(int argc, char *argv[]) {
    printf("=== Git 版本控制演示 ===\n");
    printf("这是一个被 Git 管理的示例项目。\n");
    printf("用 'git log --oneline' 查看提交历史。\n");
    printf("用 'git diff HEAD~1' 查看最近变更。\n\n");

    if (argc > 1) {
        int score = compute_score(argv[1]);
        printf("[score] input=\"%s\" → score=%d\n", argv[1], score);
    } else {
        printf("[score] no input → score=%d (NULL safe, fixed in v1.2)\n",
               compute_score(NULL));
    }

    printf("\n=== PASS ===\n");
    return 0;
}
