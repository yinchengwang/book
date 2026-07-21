#include <iostream>
using namespace std;

void ArithmeticOperators()
{
    int a = 17, b = 5;

    // 输出：22
    cout << a + b << endl;
    // 输出：12
    cout << a - b << endl;
    // 输出：85
    cout << a * b << endl;
    // 输出：3（整数除法，截断小数）
    cout << a / b << endl;
    // 输出：2（取余数，17 = 5 * 3 + 2）
    cout << a % b << endl;

    // 自增自减
    int c = 10;
    c++;
    // 输出：11
    cout << c << endl;
    c--;
    // 输出：10
    cout << c << endl;

    // 复合赋值
    c += 5;
    // 输出：15
    cout << c << endl;
    c *= 2;
    // 输出：30
    cout << c << endl;
}

void ComparisonAndLogicalOperators()
{
   int a = 10, b = 20;

    // 比较运算符
    // 输出：0（false）
    cout << (a == b) << endl;
    // 输出：1（true）
    cout << (a != b) << endl;
    // 输出：1（true）
    cout << (a < b) << endl;

    // 逻辑运算符
    // &&：两个都为真才是真
    // 输出：1
    cout << (a > 5 && b > 15) << endl;
    // ||：有一个为真就是真
    // 输出：1
    cout << (a > 100 || b > 15) << endl;
    // !：取反
    // 输出：0
    cout << !(a > 5) << endl;
}

void TernaryOperator()
{
    int a = 10, b = 20;
    // 三元运算符：条件 ? 表达式1 : 表达式2
    int max = (a > b) ? a : b;
    // 输出：20
    cout << max << endl;
}

void BitwiseOperations()
{
    int a = 10;  // 二进制 1010
    int b = 12;  // 二进制 1100

    // 按位与：两个都是 1 才是 1
    // 1010 & 1100 = 1000（十进制 8）
    // 输出：8
    cout << (a & b) << endl;

    // 按位或：有一个是 1 就是 1
    // 1010 | 1100 = 1110（十进制 14）
    // 输出：14
    cout << (a | b) << endl;

    // 按位异或：不同为 1，相同为 0
    // 1010 ^ 1100 = 0110（十进制 6）
    // 输出：6
    cout << (a ^ b) << endl;

    // 左移：相当于乘以 2
    // 输出：20
    cout << (a << 1) << endl;
    // 右移：相当于除以 2
    // 输出：5
    cout << (a >> 1) << endl;

    // 一个经典用法：判断奇偶
    // n & 1 等价于 n % 2
    int n = 7;
    if ((n & 1) == 1) {
        cout << n << " 是奇数" << endl;
    }
}

int main()
{
    ArithmeticOperators();
    ComparisonAndLogicalOperators();
    TernaryOperator();
    BitwiseOperations();

    return 0;
}