// C++ 标准库，没有 .h
#include <iostream>
#include <vector>
#include <algorithm>

// C 标准库在 C++ 中的新名字：去掉 .h，前面加 c
#include <cstdio>     // 对应 C 的 stdio.h
#include <cstdlib>    // 对应 C 的 stdlib.h
#include <cstring>    // 对应 C 的 string.h
#include <cmath>      // 对应 C 的 math.h

// 自定义头文件用双引号
// #include "utils/math_utils.h"

// 万能头文件, 一行搞定，不用记任何头文件名
#include <bits/stdc++.h>

using namespace std;

// 示例：factorial 函数（演示自定义函数）
int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; i++) result *= i;
    return result;
}

// 示例：isPrime 函数（演示自定义函数）
bool isPrime(int n) {
    if (n < 2) return false;
    for (int i = 2; i * i <= n; i++)
        if (n % i == 0) return false;
    return true;
}

int main() {
    // factorial 和 isPrime 来自自定义的 math_utils.h
    cout << "5! = " << factorial(5) << endl;
    cout << "7 is prime: " << (isPrime(7) ? "true" : "false") << endl;
    cout << "9 is prime: " << (isPrime(9) ? "true" : "false") << endl;

    // sqrt 来自标准库 cmath
    cout << "sqrt(144) = " << (int)sqrt(144) << endl;

    return 0;
}
