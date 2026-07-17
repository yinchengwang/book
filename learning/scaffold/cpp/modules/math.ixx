// modules scaffold — C++20 模块
//
// 复现命令：
//   g++ -std=c++20 -fmodules-ts -c math.ixx -o math.pcm
//   g++ -std=c++20 -fmodules-ts -fprebuilt-module-path=. main.cpp math.pcm -o test && ./test
//
// 注意：GCC 14+ 已移除 -fmodules-ts，模块支持仍在开发中。
//       推荐使用 Clang 16+ 或 MSVC 19.29+ 进行测试。
//
// 演示 3 段：
//   [export]       — 模块接口导出
//   [import]       — 模块导入
//   [fragment]     — 模块分区（Fragment）

// ============================================================================
// math.ixx — 数学模块接口
// ============================================================================
module;  // 全局模块片段开始

#include <cmath>
#include <numbers>

export module math;

// --- 模块实现片段 ---
module :private;

// 内部实现（对用户不可见）
static double deg2rad_internal(double deg) {
    return deg * std::numbers::pi / 180.0;
}

// --- 导出片段 ---
export {
    // 计算平方
    double square(double x) {
        return x * x;
    }

    // 计算立方
    double cube(double x) {
        return x * x * x;
    }

    // 阶乘（递归）
    long long factorial(int n) {
        return n <= 1 ? 1 : n * factorial(n - 1);
    }

    // 判断素数
    bool is_prime(int n) {
        if (n < 2) return false;
        for (int i = 2; i * i <= n; ++i) {
            if (n % i == 0) return false;
        }
        return true;
    }

    // 角度转弧度
    double deg2rad(double deg) {
        return deg2rad_internal(deg);  // 调用内部实现
    }

    // 弧度转角度
    double rad2deg(double rad) {
        return rad * 180.0 / std::numbers::pi;
    }
}
