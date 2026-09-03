/**
 * @file benchmark_main.cpp
 * @brief SQL 执行引擎和向量索引基准测试入口
 *
 * 初始化测试环境，运行所有基准测试，输出 JSON 结果报告。
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>

#include "benchmark_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 声明外部基准测试函数（在各自的 .cpp 文件中定义） */
void RunScanBenchmarks(BenchmarkRecorder &recorder);
void RunJoinBenchmarks(BenchmarkRecorder &recorder);
void RunAggregateBenchmarks(BenchmarkRecorder &recorder);
void RunVectorBenchmarks(BenchmarkRecorder &recorder);

#ifdef __cplusplus
}
#endif

/* 全局基准配置 */
static BenchmarkConfig g_bench_config;
static BenchmarkRecorder g_recorder;

/**
 * @brief 基准测试环境
 *
 * 初始化存储子系统，提供全局配置
 */
class BenchmarkEnvironment : public ::testing::Environment {
public:
    BenchmarkEnvironment() {}

    void SetUp() override {
        printf("\n");
        printf("==================================================\n");
        printf("      SQL 执行引擎和向量索引基准测试             \n");
        printf("==================================================\n");
        printf("\n");

        /* 初始化全局配置 */
        g_bench_config.nrows = 100000;       /* 默认 100K 行 */
        g_bench_config.ncols = 10;           /* 默认 10 列 */
        g_bench_config.warmup_runs = 3;      /* 预热 3 次 */
        g_bench_config.measure_runs = 5;     /* 测量 5 次 */
        g_bench_config.output_file = "benchmark_results.json";

        printf("配置:\n");
        printf("  默认数据规模: %lld 行 x %lld 列\n",
               (long long)g_bench_config.nrows, (long long)g_bench_config.ncols);
        printf("  预热次数: %d\n", g_bench_config.warmup_runs);
        printf("  测量次数: %d\n", g_bench_config.measure_runs);
        printf("  输出文件: %s\n", g_bench_config.output_file);
        printf("\n");
    }

    void TearDown() override {
        /* 打印汇总报告 */
        g_recorder.PrintSummary();

        /* 输出 JSON 结果 */
        g_recorder.WriteJson(g_bench_config.output_file);

        printf("\n基准测试完成。\n");
    }
};

/**
 * @brief 主函数
 *
 * 注册基准测试环境和测试用例
 */
int main(int argc, char **argv) {
    /* 初始化 Google Test */
    ::testing::InitGoogleTest(&argc, argv);

    /* 注册全局环境 */
    ::testing::AddGlobalTestEnvironment(new BenchmarkEnvironment());

    /* 运行所有测试 */
    return RUN_ALL_TESTS();
}