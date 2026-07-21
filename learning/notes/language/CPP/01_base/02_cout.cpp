#include <iostream>
#include <iomanip>
using namespace std;

int main() {
    double pi = 3.14159265;

    // 默认输出
    // 输出：3.14159
    cout << pi << endl;

    // fixed + setprecision 控制小数位数
    // 输出：3.14
    cout << fixed << setprecision(2) << pi << endl;

    // 输出：3.1416（四舍五入到 4 位小数）
    cout << fixed << setprecision(4) << pi << endl;

    return 0;
}