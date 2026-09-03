/**
 * profiling_cpp - C++ 性能分析演示
 *
 * 演示 CPU 热点（计算密集型）和 I/O 热点的创建与测量。
 * 包含 getrusage-based 计时，支持 Linux/macOS/Windows。
 */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============== 跨平台计时支持 ==============
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <processthreadsapi.h>
    #include <profileapi.h>
    #include <process.h>
#else
    #include <sys/resource.h>
    #include <unistd.h>
#endif

/** 毫秒精度计时器（跨平台） */
static double now_ms() {
#ifdef _WIN32
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return static_cast<double>(cnt.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart);
#else
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return static_cast<double>(ru.ru_utime.tv_sec) * 1000.0 +
           static_cast<double>(ru.ru_utime.tv_usec) / 1000.0;
#endif
}

// ============== CPU 热点函数（计算密集型） ==============

/**
 * CPU 密集型函数：计算 sqrt + pow + sin 的混合数学运算
 * 使用 volatile 变量防止编译器优化掉计算结果。
 */
static double compute_intensive_impl(volatile double *arr, size_t n) {
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double x = arr[i];
        // 故意引入多个浮点运算，制造微架构级别的热点
        x = std::sqrt(x + 1.0);
        x = std::pow(x, 2.5);
        x = std::sin(x) + std::cos(x);
        sum += x;
    }
    return sum;
}

/**
 * 多次调用 compute_intensive_impl，放大热点便于 perf 观测。
 */
static double compute_bound(size_t iterations, size_t n) {
    double *arr = new double[n];
    for (size_t i = 0; i < n; ++i) {
        arr[i] = static_cast<double>(i + 1);
    }

    volatile double result = 0.0;
    for (size_t k = 0; k < iterations; ++k) {
        result += compute_intensive_impl(arr, n);
    }

    delete[] arr;
    return result;
}

// ============== I/O 热点函数 ==============

/**
 * I/O 密集型函数：模拟写入延迟。
 * 纯计算下 CPU 占用极低，用于展示 perf top 中进程状态的差异。
 */
static void io_bound(const char *path, size_t lines) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror("fopen");
        return;
    }
    for (size_t i = 0; i < lines; ++i) {
        fprintf(fp, "Line %08zu: Lorem ipsum dolor sit amet.\n", i);
    }
    fclose(fp);
}

// ============== 主程序入口 ==============
int main(int argc, char *argv[]) {
    (void)argc;  // 未使用
    (void)argv;

    printf("=== C++ 性能分析演示 ===\n");
    printf("PID: %d\n", static_cast<int>(getpid()));
    printf("\n");

    // -------- CPU 热点：计算密集型 --------
    {
        const size_t N = 500000;       // 数组大小
        const size_t ITER = 20;         // 迭代次数（放大热点）
        printf("[1] CPU 热点测试（compute_bound）：\n");
        printf("    N=%zu, iterations=%zu\n", N, ITER);

        double t0 = now_ms();
        volatile double r = compute_bound(ITER, N);
        double t1 = now_ms();

        printf("    结果: %.2f, 耗时: %.2f ms\n", r, t1 - t0);
        printf("    -> 预期 perf top: 大量时间花在 sqrt/pow/sin\n");
        printf("\n");
    }

    // -------- I/O 热点 --------
    {
        const char *tmpfile = "/tmp/profiling_io_demo.txt";
        const size_t LINES = 100000;
        printf("[2] I/O 热点测试（io_bound）：\n");
        printf("    输出文件: %s, 行数: %zu\n", tmpfile, LINES);

        double t0 = now_ms();
        io_bound(tmpfile, LINES);
        double t1 = now_ms();

        printf("    耗时: %.2f ms\n", t1 - t0);
        printf("    -> 预期 perf top: CPU 占用低，大部分时间在 sys_write\n");
        printf("\n");
    }

    printf("运行 'perf top -p %d' 观察 CPU 热点。\n", static_cast<int>(getpid()));
    printf("运行 'perf record -F 99 -g -p %d && perf report -g' 采样热点函数。\n",
           static_cast<int>(getpid()));
    return 0;
}
