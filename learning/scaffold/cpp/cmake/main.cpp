// cmake scaffold — CMake 构建系统演示
//
// 复现命令（两种方式）：
//   方式 1：直接 g++（无需 cmake）
//     g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
//
//   方式 2：用 CMake 构建（推荐）
//     mkdir -p build && cd build
//     cmake ..
//     make
//     ./cpp_cmake_demo
//
// 演示 4 段：
//   [greet]   — 简单函数
//   [math]    — 加减乘除四件套
//   [config]  — 读取 CMake 定义的宏（版本号、构建类型）
//   [compile] — 演示条件编译

#include <iostream>
#include <string>
#include <cstdio>

// 由 CMake 通过 add_definitions(-DAPP_VERSION="...") 注入
#ifndef APP_VERSION
  #define APP_VERSION "unknown"
#endif
#ifndef BUILD_TYPE
  #define BUILD_TYPE "default"
#endif

// [greet] 函数
static void greet(const std::string& name) {
    printf("  [greet] Hello, %s!\n", name.c_str());
}

// [math] 四则运算
static double add(double a, double b) { return a + b; }
static double sub(double a, double b) { return a - b; }
static double mul(double a, double b) { return a * b; }
static double divide_fn(double a, double b) {
    if (b == 0) return 0;
    return a / b;
}

int main() {
    // === [config] CMake 注入的配置 ===
    printf("[config] === CMake 注入的配置 ===\n");
    printf("  APP_VERSION = \"%s\"\n", APP_VERSION);
    printf("  BUILD_TYPE  = \"%s\"\n", BUILD_TYPE);

    // === [greet] 函数调用 ===
    printf("\n[greet] === 函数调用 ===\n");
    greet("World");
    greet("CMake");

    // === [math] 四则运算 ===
    printf("\n[math] === 四则运算 ===\n");
    double a = 10.0, b = 3.0;
    printf("  %.2f + %.2f = %.2f\n", a, b, add(a, b));
    printf("  %.2f - %.2f = %.2f\n", a, b, sub(a, b));
    printf("  %.2f * %.2f = %.2f\n", a, b, mul(a, b));
    printf("  %.2f / %.2f = %.2f\n", a, b, divide_fn(a, b));

    // === [compile] 条件编译演示 ===
    printf("\n[compile] === 条件编译 ===\n");
#ifdef NDEBUG
    printf("  NDEBUG 已定义（release 构建）\n");
#else
    printf("  NDEBUG 未定义（debug 构建，含 assert）\n");
#endif

    printf("\n=== PASS ===\n");
    return 0;
}