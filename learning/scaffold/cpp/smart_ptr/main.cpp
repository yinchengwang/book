// smart_ptr scaffold — 智能指针
//
// 复现命令：
//   g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
//
// 演示 5 段：
//   [unique]    — std::unique_ptr 独占所有权
//   [shared]    — std::shared_ptr 共享所有权（引用计数）
//   [weak]      — std::weak_ptr 解决循环引用
//   [deleter]   — 自定义删除器（FILE*/FILE fd）
//   [make]      — std::make_unique / std::make_shared（C++14）

#include <iostream>
#include <memory>
#include <string>
#include <cstdio>
#include <vector>

// === 资源类 ===
struct Resource {
    int id;
    Resource(int i) : id(i) {
        printf("  [ctor] Resource(%d)\n", id);
    }
    ~Resource() {
        printf("  [dtor] ~Resource(%d)\n", id);
    }
    void hello() const {
        printf("  Resource(%d) hello\n", id);
    }
};

// 自定义删除器：FILE*
struct FileCloser {
    void operator()(FILE* fp) const {
        if (fp) {
            fclose(fp);
            printf("  [deleter] fclose\n");
        }
    }
};

int main() {
    // === [unique] unique_ptr 独占 ===
    printf("[unique] === std::unique_ptr ===\n");
    {
        std::unique_ptr<Resource> p1(new Resource(1));
        p1->hello();
        printf("  unique_ptr 无引用计数（独占所有权）\n");

        // 不可拷贝
        // std::unique_ptr<Resource> p2 = p1;  // 编译错误

        // 可移动
        std::unique_ptr<Resource> p2 = std::move(p1);
        printf("  p2 = std::move(p1): p1 is %s, p2->id=%d\n",
               p1 ? "valid" : "null", p2->id);

        // p2 离开作用域自动 delete Resource(1)
    }

    // === [make] std::make_unique（C++14）===
    printf("\n[make] === std::make_unique ===\n");
    {
        auto p = std::make_unique<Resource>(2);
        p->hello();
        printf("  make_unique 比 new 更安全（异常安全）\n");
    }

    // === [shared] shared_ptr 共享 ===
    printf("\n[shared] === std::shared_ptr ===\n");
    {
        std::shared_ptr<Resource> p1 = std::make_shared<Resource>(3);
        printf("  p1.use_count = %ld\n", (long)p1.use_count());

        {
            std::shared_ptr<Resource> p2 = p1;
            printf("  p2 = p1: use_count = %ld\n", (long)p1.use_count());
        }
        printf("  p2 离开作用域: use_count = %ld\n", (long)p1.use_count());
    }
    printf("  引用计数归零，Resource(3) 被销毁\n");

    // === [weak] weak_ptr 解决循环引用 ===
    printf("\n[weak] === std::weak_ptr 防循环引用 ===\n");
    {
        struct Node {
            std::shared_ptr<Node> next;
            std::weak_ptr<Node> prev;       // weak 防循环
            int id;
            Node(int i) : id(i) {
                printf("  [ctor] Node(%d)\n", id);
            }
            ~Node() {
                printf("  [dtor] ~Node(%d)\n", id);
            }
        };

        auto a = std::make_shared<Node>(10);
        auto b = std::make_shared<Node>(20);
        a->next = b;
        b->prev = a;                       // weak 不增加 refcount
        printf("  a.use_count = %ld, b.use_count = %ld\n",
               (long)a.use_count(), (long)b.use_count());

        // weak_ptr 必须 lock() 转 shared_ptr 才能用
        if (auto p = b->prev.lock()) {
            printf("  b->prev.lock(): id=%d\n", p->id);
        }
        // 离开作用域：a 和 b 各一次销毁（无循环泄漏）
    }

    // === [deleter] 自定义删除器 ===
    printf("\n[deleter] === 自定义删除器 ===\n");
    {
        std::unique_ptr<FILE, FileCloser> fp(fopen("cpp_smart_ptr_demo.tmp", "w"));
        if (fp) {
            fputs("managed by unique_ptr<FILE, FileCloser>\n", fp.get());
            printf("  fwrite OK\n");
            // 离开作用域自动 fclose（FileCloser 删除器）
        }
    }
    std::remove("cpp_smart_ptr_demo.tmp");

    // === [deleter-2] lambda 删除器 ===
    printf("\n[deleter-2] === lambda 删除器 ===\n");
    {
        auto closer = [](FILE* fp) {
            if (fp) {
                fclose(fp);
                printf("  [lambda-deleter] fclose\n");
            }
        };
        std::unique_ptr<FILE, decltype(closer)> fp(fopen("cpp_smart_ptr_demo2.tmp", "w"), closer);
        if (fp) {
            fputs("lambda deleter demo\n", fp.get());
        }
    }
    std::remove("cpp_smart_ptr_demo2.tmp");

    printf("\n=== PASS ===\n");
    return 0;
}