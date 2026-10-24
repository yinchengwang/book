/**
 * @file benchmark_config.h
 * @brief SQL 执行引擎和向量索引性能基准测试配置框架
 *
 * 提供基准测试的基础设施：
 * - BenchmarkConfig: 测试数据配置
 * - BenchmarkResult: 测试结果记录
 * - BenchmarkRecorder: 结果记录器和汇总报告
 * - BenchmarkTimer: 高精度计时器
 * - RunBenchmark: 模板化的基准测试运行框架
 */

#ifndef BENCHMARK_CONFIG_H
#define BENCHMARK_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 测试数据配置
 * ============================================================ */

/**
 * @brief 基准测试配置
 */
typedef struct BenchmarkConfig {
    int64_t nrows;              /**< 数据行数 */
    int64_t ncols;              /**< 数据列数 */
    int32_t warmup_runs;        /**< 预热运行次数 */
    int32_t measure_runs;       /**< 测量运行次数 */
    const char *output_file;    /**< JSON 输出文件路径 */
} BenchmarkConfig;

/**
 * @brief 单次基准测试结果
 */
typedef struct BenchmarkResult {
    const char *name;           /**< 测试名称 */
    double duration_ms;         /**< 平均耗时（毫秒） */
    double min_ms;              /**< 最小耗时 */
    double max_ms;              /**< 最大耗时 */
    double rows_per_sec;        /**< 每秒处理行数 */
    bool passed;                /**< 是否通过性能阈值 */
} BenchmarkResult;

#ifdef __cplusplus
}
#endif

/* ============================================================
 * C++ 基准测试框架
 * ============================================================ */

#ifdef __cplusplus

#include <fstream>
#include <sstream>
#include <iomanip>

/**
 * @brief 基准测试结果记录器
 *
 * 收集测试结果、打印汇总、输出 JSON
 */
class BenchmarkRecorder {
public:
    /**
     * @brief 添加测试结果
     */
    void AddResult(const BenchmarkResult &result) {
        results_.push_back(result);
    }

    /**
     * @brief 打印汇总报告到 stdout
     */
    void PrintSummary() const {
        printf("\n");
        printf("==================================================\n");
        printf("              基准测试结果汇总                   \n");
        printf("==================================================\n");
        printf("\n");

        int passed = 0;
        int failed = 0;

        for (const auto &r : results_) {
            printf("测试: %s\n", r.name);
            printf("  平均耗时: %.3f ms\n", r.duration_ms);
            printf("  最小耗时: %.3f ms\n", r.min_ms);
            printf("  最大耗时: %.3f ms\n", r.max_ms);
            if (r.rows_per_sec > 0) {
                printf("  吞吐量:   %.0f 行/秒\n", r.rows_per_sec);
            }
            printf("  状态:     %s\n", r.passed ? "通过" : "未达标");
            printf("\n");

            if (r.passed) {
                passed++;
            } else {
                failed++;
            }
        }

        printf("--------------------------------------------------\n");
        printf("总计: %d 通过, %d 未达标, %zu 个测试\n",
               passed, failed, results_.size());
        printf("==================================================\n");
    }

    /**
     * @brief 写入 JSON 结果文件
     *
     * 格式:
     * {
     *   "results": [
     *     {
     *       "name": "BenchmarkScanTest.SeqScan100K",
     *       "duration_ms": 150.5,
     *       "min_ms": 140.2,
     *       "max_ms": 165.3,
     *       "rows_per_sec": 665556.7,
     *       "passed": true
     *     },
     *     ...
     *   ],
     *   "summary": {
     *     "total": 10,
     *     "passed": 9,
     *     "failed": 1
     *   }
     * }
     */
    void WriteJson(const char *filename) const {
        if (!filename) {
            fprintf(stderr, "警告: JSON 输出文件名为空，跳过写入\n");
            return;
        }

        std::ofstream out(filename);
        if (!out.is_open()) {
            fprintf(stderr, "错误: 无法打开文件 %s\n", filename);
            return;
        }

        int passed = 0;
        int failed = 0;
        for (const auto &r : results_) {
            if (r.passed) passed++;
            else failed++;
        }

        out << std::fixed << std::setprecision(3);
        out << "{\n";
        out << "  \"results\": [\n";

        for (size_t i = 0; i < results_.size(); i++) {
            const auto &r = results_[i];
            out << "    {\n";
            out << "      \"name\": \"" << r.name << "\",\n";
            out << "      \"duration_ms\": " << r.duration_ms << ",\n";
            out << "      \"min_ms\": " << r.min_ms << ",\n";
            out << "      \"max_ms\": " << r.max_ms << ",\n";
            out << "      \"rows_per_sec\": " << r.rows_per_sec << ",\n";
            out << "      \"passed\": " << (r.passed ? "true" : "false") << "\n";
            out << "    }";
            if (i < results_.size() - 1) {
                out << ",";
            }
            out << "\n";
        }

        out << "  ],\n";
        out << "  \"summary\": {\n";
        out << "    \"total\": " << results_.size() << ",\n";
        out << "    \"passed\": " << passed << ",\n";
        out << "    \"failed\": " << failed << "\n";
        out << "  }\n";
        out << "}\n";

        out.close();
        printf("基准测试结果已写入: %s\n", filename);
    }

private:
    std::vector<BenchmarkResult> results_;
};

/**
 * @brief 高精度计时器
 *
 * 使用 std::chrono 实现毫秒级精度计时
 */
class BenchmarkTimer {
public:
    BenchmarkTimer() : started_(false), start_time_(), end_time_() {}

    /**
     * @brief 开始计时
     */
    void Start() {
        start_time_ = std::chrono::high_resolution_clock::now();
        started_ = true;
    }

    /**
     * @brief 停止计时
     */
    void Stop() {
        end_time_ = std::chrono::high_resolution_clock::now();
        started_ = false;
    }

    /**
     * @brief 获取经过的时间（毫秒）
     */
    double GetElapsedMs() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time_ - start_time_);
        return duration.count() / 1000.0;
    }

private:
    bool started_;
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
};

/**
 * @brief 运行基准测试的模板函数
 *
 * 流程：
 * 1. 预热阶段：运行 warmup_runs 次预热
 * 2. 测量阶段：运行 measure_runs 次实际测量
 * 3. 统计计算：计算平均值、最小值、最大值、吞吐量
 * 4. 达标检查：根据阈值检查是否达标
 *
 * @param name 测试名称
 * @param config 测试配置
 * @param setup_func 初始化函数（准备测试数据）
 * @param bench_func 基准函数（实际测试逻辑）
 * @param teardown_func 清理函数
 * @param threshold_ms 性能阈值（毫秒），0 表示不检查
 * @param recorder 结果记录器
 */
template <typename SetupFunc, typename BenchFunc, typename TeardownFunc>
void RunBenchmark(const char *name,
                  const BenchmarkConfig &config,
                  SetupFunc setup_func,
                  BenchFunc bench_func,
                  TeardownFunc teardown_func,
                  double threshold_ms,
                  BenchmarkRecorder &recorder) {
    printf("运行基准测试: %s\n", name);
    printf("  数据规模: %lld 行 x %lld 列\n",
           (long long)config.nrows, (long long)config.ncols);
    printf("  预热次数: %d\n", config.warmup_runs);
    printf("  测量次数: %d\n", config.measure_runs);

    /* 预热阶段 */
    printf("  预热中...\n");
    for (int32_t i = 0; i < config.warmup_runs; i++) {
        setup_func();
        bench_func();
        teardown_func();
    }

    /* 测量阶段 */
    printf("  测量中...\n");
    std::vector<double> durations;
    durations.reserve(config.measure_runs);

    for (int32_t i = 0; i < config.measure_runs; i++) {
        setup_func();

        BenchmarkTimer timer;
        timer.Start();
        bench_func();
        timer.Stop();

        durations.push_back(timer.GetElapsedMs());
        teardown_func();
    }

    /* 统计计算 */
    double sum = std::accumulate(durations.begin(), durations.end(), 0.0);
    double avg_ms = sum / durations.size();
    double min_ms = *std::min_element(durations.begin(), durations.end());
    double max_ms = *std::max_element(durations.begin(), durations.end());

    double rows_per_sec = 0.0;
    if (avg_ms > 0 && config.nrows > 0) {
        rows_per_sec = (config.nrows * 1000.0) / avg_ms;
    }

    /* 达标检查 */
    bool passed = true;
    if (threshold_ms > 0) {
        passed = (avg_ms <= threshold_ms);
        if (!passed) {
            printf("  警告: 平均耗时 %.3f ms 超过阈值 %.3f ms\n",
                   avg_ms, threshold_ms);
        }
    }

    /* 记录结果 */
    BenchmarkResult result;
    result.name = name;
    result.duration_ms = avg_ms;
    result.min_ms = min_ms;
    result.max_ms = max_ms;
    result.rows_per_sec = rows_per_sec;
    result.passed = passed;

    recorder.AddResult(result);

    printf("  完成: 平均 %.3f ms, 最小 %.3f ms, 最大 %.3f ms\n",
           avg_ms, min_ms, max_ms);
    if (rows_per_sec > 0) {
        printf("  吞吐量: %.0f 行/秒\n", rows_per_sec);
    }
    printf("\n");
}

#endif /* __cplusplus */

#endif /* BENCHMARK_CONFIG_H */