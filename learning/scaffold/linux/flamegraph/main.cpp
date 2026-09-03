/**
 * @file main.cpp
 * @brief CPU 热点火焰图演示 - 3 层调用链
 *
 * 展示真实的调用栈模式: main → process_data → compute_heavy → do_math
 * 适用于 perf + FlameGraph 生成火焰图
 *
 * @note 需要 Linux/WSL 环境运行 perf 采样
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

// ========== 时间工具 ==========

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double, std::milli>;

static double get_timestamp_ms() {
    auto now = Clock::now();
    return std::chrono::duration<double, std::milli>(
        now.time_since_epoch()
    ).count();
}

// ========== 数学计算（叶子热点） ==========

/**
 * 模拟 CPU 密集型数学运算
 * 这是火焰图的叶子节点，真正消耗 CPU 的地方
 */
static double do_math(double value, int iterations) {
    double result = value;
    for (int i = 0; i < iterations; ++i) {
        result = std::sqrt(result + 1.0);
        result = std::pow(result, 1.0001);
        result = std::sin(result) + std::cos(result);
    }
    return result;
}

// ========== 计算密集层 ==========

/**
 * 中间层：处理一组数据的 CPU 密集计算
 */
static double compute_heavy(const std::vector<double>& data) {
    double sum = 0.0;
    for (size_t i = 0; i < data.size(); ++i) {
        // 调用叶子函数，模拟真实调用栈
        double val = do_math(data[i], 1000);
        sum += val;

        // 额外的 CPU 操作
        for (int j = 0; j < 100; ++j) {
            sum += std::log(std::abs(val) + 1.0);
        }
    }
    return sum;
}

// ========== 数据处理层 ==========

/**
 * 模拟 I/O 等待 + CPU 处理混合负载
 * 这是火焰图中间层，连接 main 和 compute_heavy
 */
static std::vector<double> process_data(const std::string& filename) {
    std::vector<double> result;
    double start = get_timestamp_ms();

    // 模拟 I/O 操作 (sleep 模拟磁盘/网络延迟)
    sleep_ms(10);

    // 生成模拟数据
    for (int i = 0; i < 5000; ++i) {
        double val = static_cast<double>(i) * 0.01;
        result.push_back(val);
    }

    // CPU 密集处理（调用 compute_heavy）
    double total = compute_heavy(result);

    // 再次模拟 I/O
    sleep_ms(10);

    double elapsed = get_timestamp_ms() - start;
    printf("[process_data] 处理 %zu 条数据，耗时 %.2f ms，总和=%.2f\n",
           result.size(), elapsed, total);

    return result;
}

// ========== 主函数 ==========

static void print_banner() {
    printf("========================================\n");
    printf("   CPU 热点火焰图演示程序\n");
    printf("   调用链: main → process_data →\n");
    printf("          compute_heavy → do_math\n");
    printf("========================================\n\n");
}

static void print_usage(const char* prog) {
    printf("用法: %s [数据文件]\n", prog);
    printf("\n示例:\n");
    printf("  %s                  # 使用内置模拟数据\n", prog);
    printf("  %s data.txt         # 读取文件（会读取但用模拟数据）\n", prog);
}

int main(int argc, char* argv[]) {
    double program_start = get_timestamp_ms();

    print_banner();

    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    printf("程序启动时间戳: %.2f ms\n\n", program_start);

    // 运行多次以产生足够的采样数据
    const int iterations = 5;
    double total_result = 0.0;

    for (int iter = 0; iter < iterations; ++iter) {
        double iter_start = get_timestamp_ms();

        printf("--- 第 %d/%d 次迭代 ---\n", iter + 1, iterations);

        // 模拟不同场景的数据处理
        std::string filename = (argc > 1) ? argv[1] : "模拟数据";

        // 调用 3 层调用链
        auto data = process_data(filename);

        // 在主函数层也做些处理
        for (size_t i = 0; i < data.size() / 10; ++i) {
            total_result += data[i] * 0.001;
        }

        double iter_elapsed = get_timestamp_ms() - iter_start;
        printf("第 %d 次迭代耗时: %.2f ms\n\n", iter + 1, iter_elapsed);
    }

    double program_elapsed = get_timestamp_ms() - program_start;
    printf("========================================\n");
    printf("程序执行完成\n");
    printf("总耗时: %.2f ms\n", program_elapsed);
    printf("总结果: %.6f\n", total_result);
    printf("========================================\n");

    return 0;
}
