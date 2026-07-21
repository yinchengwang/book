#include <iostream>
#include <string>
#include <vector>

using namespace std;

// 定义一个函数：计算两个数的和
// int 是返回类型，add 是函数名，(int a, int b) 是参数列表
int add(int a, int b)
{
    return a + b;
}

// 没有返回值的函数，用 void
void greet(string name)
{
    cout << "你好，" << name << "！" << endl;
}

void tryChange(int x)
{
    x = 999;
    // 输出：函数内 x = 999
    cout << "函数内 x = " << x << endl;
}

void PassByValue()
{
    int a = 10;
    tryChange(a);
    // 输出：a = 10
    cout << "a = " << a << endl;
}

// 参数类型后面加 & 表示引用传递
void changeValue(int& x)
{
    x = 999;
}

// 经典用法：交换两个变量的值
void swap(int& a, int& b)
{
    int temp = a;
    a = b;
    b = temp;
}

void PassByReference()
{
    int a = 10;
    changeValue(a);
    // 输出：a = 999
    cout << "a = " << a << endl;

    int x = 1, y = 2;
    swap(x, y);
    // 输出：x = 2, y = 1
    cout << "x = " << x << ", y = " << y << endl;
}

void base()
{
    // 调用函数
    int result = add(3, 5);
    // 输出：8
    cout << result << endl;

    // 输出：你好，C++！
    greet("C++");

    // 函数的返回值可以直接参与运算
    // 输出：19
    cout << add(10, 9) << endl;
}

// 正确做法：用引用传递
void dfs(vector<int>& path, int depth) {
    if (depth == 0) {
        // 输出路径
        for (int x : path) {
            cout << x << " ";
        }
        cout << endl;
        return;
    }
    for (int i = 1; i <= 3; i++) {
        path.push_back(i);
        dfs(path, depth - 1);
        path.pop_back();
    }
}

void PassContainerRecursively()
{
    vector<int> path;
    dfs(path, 2);
    return;
}

// ========== 重载（Overload）==========
// C++ 支持函数重载：同名函数，参数不同

// 三个 int 相加
int add(int a, int b, int c)
{
    return a + b + c;
}

// 两个 double 相加
double add(double a, double b)
{
    return a + b;
}

// C++ 允许多个函数同名，只要参数列表不同（参数个数不同，或参数类型不同）
// 编译器会根据调用时传入的参数自动选择正确的版本。这就是函数重载：
void FunctionOverloading()
{
    // 调用 add(int, int)
    // 输出：3
    cout << add(1, 2) << endl;

    // 调用 add(int, int, int)
    // 输出：6
    cout << add(1, 2, 3) << endl;

    // 调用 add(double, double)
    // 输出：3.3
    cout << add(1.1, 2.2) << endl;
}

int main()
{
    base();

    // 基本类型传值（副本）
    PassByValue();
    // 容器类型传引用（避免复制），如果函数只是读取容器、不需要修改它，就加上 const 表示"只读引用"
    PassByReference();
    // 使用引用传递容器，避免多次复制
    PassContainerRecursively();

    return 0;
}