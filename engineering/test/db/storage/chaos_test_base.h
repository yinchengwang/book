/**
 * @file chaos_test_base.h
 * @brief 混沌测试基类 - 用于崩溃恢复测试
 *
 * 提供测试目录管理和模拟崩溃功能。
 */
#ifndef CHAOS_TEST_BASE_H
#define CHAOS_TEST_BASE_H

#include <gtest/gtest.h>
#include <string>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

/**
 * @brief 混沌测试基类
 *
 * 每个测试用例在 test-results/chaos/<test_name>/ 下创建独立目录。
 * 提供 simulate_crash 模板函数用于模拟崩溃。
 */
class ChaosTestBase : public ::testing::Test {
protected:
    std::string test_dir;

    void SetUp() override {
        /* 创建测试目录 ./test-results/chaos/<test_name>/ */
        test_dir = std::string("./test-results/chaos/") +
                   ::testing::UnitTest::GetInstance()->current_test_info()->name();
        fs::remove_all(test_dir);
        fs::create_directories(test_dir);
    }

    void TearDown() override {
        /* 清理测试目录 */
        fs::remove_all(test_dir);
    }

    /**
     * @brief 模拟崩溃：直接销毁实例，不保存状态
     *
     * 调用销毁函数但不等待刷盘完成，模拟崩溃场景。
     *
     * @param obj 对象指针（会被置为 nullptr）
     * @param destroy 销毁函数
     */
    template<typename T>
    void simulate_crash(T*& obj, void (*destroy)(T*)) {
        if (obj) {
            destroy(obj);
            obj = nullptr;
        }
    }
};

#endif /* CHAOS_TEST_BASE_H */