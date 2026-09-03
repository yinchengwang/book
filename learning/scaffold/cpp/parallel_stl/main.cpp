/**
 * parallel_stl.cpp - C++17 Parallel STL 执行策略演示
 *
 * 演示 std::execution::seq/par/par_unseq 策略的用法和性能差异。
 * C++17 标准库内置，无需额外依赖。
 * 编译: g++ -std=c++17 -O2 -pthread main.cpp -o parallel_stl
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <execution>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

// 计算耗时（毫秒）
template <typename Func>
double measure_ms(Func &&func)
{
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// 生成随机数据
std::vector<double> generate_data(size_t n)
{
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> dist(0.0, 1000.0);
    std::vector<double> data(n);
    for (size_t i = 0; i < n; ++i) {
        data[i] = dist(rng);
    }
    return data;
}

// 幂运算（计算密集型操作）
double power_op(double x)
{
    return std::pow(x, 3.0) + std::sqrt(x) + std::sin(x) * std::cos(x);
}

// ========== 执行策略对比 ==========

void demo_transform(std::vector<double> &data, const std::string &label,
                    std::execution::parallel_unsequenced_policy /*unused*/)
{
    std::cout << "\n[" << label << "] Transform (par_unseq):\n";
    std::transform(std::execution::par_unseq, data.begin(), data.end(),
                   data.begin(), power_op);
}

void demo_transform(std::vector<double> &data, const std::string &label,
                    std::execution::parallel_policy /*unused*/)
{
    std::cout << "\n[" << label << "] Transform (par):\n";
    std::transform(std::execution::par, data.begin(), data.end(),
                   data.begin(), power_op);
}

void demo_transform(std::vector<double> &data, const std::string &label,
                    std::execution::sequenced_policy /*unused*/)
{
    std::cout << "\n[" << label << "] Transform (seq):\n";
    std::transform(std::execution::seq, data.begin(), data.end(),
                   data.begin(), power_op);
}

// 排序演示
template <typename Policy>
void demo_sort(std::vector<double> &data, const std::string &label, Policy policy)
{
    std::cout << "[" << label << "] Sort:\n";
    std::sort(policy, data.begin(), data.end());
}

// ========== 主函数 ==========

int main()
{
    const size_t N = 5'000'000;
    std::cout << "=== Parallel STL Execution Policies Demo ===\n";
    std::cout << "Data size: " << N << " elements\n\n";

    // 1. Transform 操作性能对比
    std::cout << "========== Transform Performance ==========\n";

    auto data_seq = generate_data(N);
    auto data_par = generate_data(N);
    auto data_pus = generate_data(N);

    double t_seq = measure_ms([&]() {
        std::transform(std::execution::seq, data_seq.begin(), data_seq.end(),
                       data_seq.begin(), power_op);
    });
    std::cout << "  seq:      " << std::fixed << std::setprecision(2) << t_seq << " ms\n";

    double t_par = measure_ms([&]() {
        std::transform(std::execution::par, data_par.begin(), data_par.end(),
                       data_par.begin(), power_op);
    });
    std::cout << "  par:      " << t_par << " ms (speedup: " << t_seq / t_par << "x)\n";

    double t_pus = measure_ms([&]() {
        std::transform(std::execution::par_unseq, data_pus.begin(), data_pus.end(),
                       data_pus.begin(), power_op);
    });
    std::cout << "  par_unseq:" << t_pus << " ms (speedup: " << t_seq / t_pus << "x)\n";

    // 2. 排序性能对比
    std::cout << "\n========== Sort Performance ==========\n";

    auto sort_seq = generate_data(N);
    auto sort_par = generate_data(N);

    double s_seq = measure_ms([&]() {
        std::sort(std::execution::seq, sort_seq.begin(), sort_seq.end());
    });
    std::cout << "  seq:      " << std::fixed << std::setprecision(2) << s_seq << " ms\n";

    double s_par = measure_ms([&]() {
        std::sort(std::execution::par, sort_par.begin(), sort_par.end());
    });
    std::cout << "  par:      " << s_par << " ms (speedup: " << s_seq / s_par << "x)\n";

    // 3. 归约操作
    std::cout << "\n========== Reduce Performance ==========\n";

    auto red_data = generate_data(N);
    double sum_seq = measure_ms([&]() {
        std::reduce(std::execution::seq, red_data.begin(), red_data.end(), 0.0);
    });
    std::cout << "  seq sum:  " << std::fixed << std::setprecision(2) << sum_seq << " ms\n";

    double sum_par = measure_ms([&]() {
        std::reduce(std::execution::par, red_data.begin(), red_data.end(), 0.0);
    });
    std::cout << "  par sum:  " << sum_par << " ms (speedup: " << sum_seq / sum_par << "x)\n";

    std::cout << "\n========== Policy Explanation ==========\n";
    std::cout << "  seq:       顺序执行，无并行\n";
    std::cout << "  par:       并行执行，线程安全保证\n";
    std::cout << "  par_unseq: 并行+向量化，最高性能但不保证线程安全\n";

    return 0;
}
