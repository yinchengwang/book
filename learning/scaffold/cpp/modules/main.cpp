// main.cpp — C++20 模块演示
//
// 复现命令：
//   g++ -std=c++20 -fmodules-ts -c math.ixx -o math.pcm
//   g++ -std=c++20 -fmodules-ts -fprebuilt-module-path=. main.cpp math.pcm -o test && ./test

import math;
#include <iostream>

int main() {
    // [export] 模块导出功能
    std::cout << "[export] === 模块导出功能 ===" << std::endl;
    std::cout << "  square(5) = " << square(5) << std::endl;
    std::cout << "  cube(3) = " << cube(3) << std::endl;
    std::cout << "  factorial(5) = " << factorial(5) << std::endl;
    std::cout << "  is_prime(17) = " << (is_prime(17) ? "true" : "false") << std::endl;

    // [import] 模块导入
    std::cout << "\n[import] === 角度/弧度转换 ===" << std::endl;
    std::cout << "  180° = " << deg2rad(180.0) << " rad (π)" << std::endl;
    std::cout << "  π rad = " << rad2deg(std::numbers::pi) << "°" << std::endl;

    // [fragment] 模块分区验证
    std::cout << "\n[fragment] === 素数判断 ===" << std::endl;
    int primes[] = {2, 17, 100};
    for (int p : primes) {
        std::cout << "  is_prime(" << p << ") = " << (is_prime(p) ? "true" : "false") << std::endl;
    }

    std::cout << "\n=== PASS ===" << std::endl;
    return 0;
}
