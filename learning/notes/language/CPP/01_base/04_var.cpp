#include <iostream>
using namespace std;

void var()
{
    // 整数
    int a = 42;
    // 长整数，注意末尾的 LL
    long long b = 9999999999LL;
    // 浮点数
    double c = 3.14;
    // 单个字符，用单引号
    char d = 'A';
    // 布尔值，true 或 false
    bool e = true;
    // 字符串，用双引号
    string s = "hello";

    cout << "int: " << a << endl;
    cout << "long long: " << b << endl;
    cout << "double: " << c << endl;
    cout << "char: " << d << endl;
    // bool 输出是 1（true）或 0（false）
    cout << "bool: " << e << endl;
    cout << "string: " << s << endl;
}

void auto_var()
{
    auto a = 42;          // 推断为 int
    auto b = 3.14;        // 推断为 double
    auto c = 'A';         // 推断为 char
    auto d = true;        // 推断为 bool
    auto s = string("hello"); // 推断为 string

    cout << a << endl;
    cout << b << endl;
    cout << c << endl;
    cout << d << endl;
    cout << s << endl;
}

int main()
{
    var();

    auto_var();

    return 0;
}