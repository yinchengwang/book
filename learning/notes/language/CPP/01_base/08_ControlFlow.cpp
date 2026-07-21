#include <iostream>
#include <vector>
using namespace std;

void IfElse()
{
    int score = 85;

    if (score >= 90) {
        cout << "优秀" << endl;
    } else if (score >= 60) {
        cout << "及格" << endl;
    } else {
        cout << "不及格" << endl;
    }
    // 输出：及格
}

void forLoop()
{
    // 基本 for 循环
    // 输出：0 1 2 3 4
    for (int i = 0; i < 5; i++) {
        cout << i << " ";
    }
    cout << endl;

    // 倒序遍历
    // 输出：5 4 3 2 1
    for (int i = 5; i >= 1; i--) {
        cout << i << " ";
    }
    cout << endl;

    // 嵌套循环：打印乘法表的一小部分
    for (int i = 1; i <= 3; i++) {
        for (int j = 1; j <= 3; j++) {
            cout << i * j << "\t";
        }
        cout << endl;
    }
}

void whileLoop()
{
    // 计算 1 + 2 + ... + 100
    int sum = 0;
    int i = 1;
    while (i <= 100) {
        sum += i;
        i++;
    }
    // 输出：5050
    cout << sum << endl;

    // do-while：至少执行一次
    int n = 0;
    do {
        cout << "执行了一次，n = " << n << endl;
    } while (n > 0);
    // 虽然 n = 0 不满足条件，但还是会执行一次
}

void ForEachLoop()
{
    // 遍历数组
    int nums[] = {10, 20, 30, 40, 50};
    // 输出：10 20 30 40 50
    for (int x : nums) {
        cout << x << " ";
    }
    cout << endl;

    // 遍历字符串中的每个字符
    string s = "hello";
    // 输出：h e l l o
    for (char c : s) {
        cout << c << " ";
    }
    cout << endl;

    // 用引用修改数组中的元素
    for (int& x : nums) {
        x *= 2;
    }
    // 输出：20 40 60 80 100
    for (int x : nums) {
        cout << x << " ";
    }
    cout << endl;
}

void BreakAndContinue()
{
    // break：找到第一个能被 7 整除的数就停止
    for (int i = 1; i <= 100; i++) {
        if (i % 7 == 0) {
            cout << "第一个能被 7 整除的数：" << i << endl;
            break;
        }
    }

    // continue：跳过偶数，只打印奇数
    // 输出：1 3 5 7 9
    for (int i = 1; i <= 10; i++) {
        if (i % 2 == 0) {
            continue;
        }
        cout << i << " ";
    }
    cout << endl;
}

void SwitchCase()
{
    int day;
    cin >> day;

    switch (day) {
        case 1:
            cout << "星期一" << endl;
            break;
        case 2:
            cout << "星期二" << endl;
            break;
        case 3:
            cout << "星期三" << endl;
            break;
        case 4:
            cout << "星期四" << endl;
            break;
        case 5:
            cout << "星期五" << endl;
            break;
        case 6:
            cout << "星期六" << endl;
            break;
        case 7:
            cout << "星期日" << endl;
            break;
        default:
            cout << "无效的输入" << endl;
            break;
    }
}

int Divide(int a, int b) {
    if (b == 0) {
        // 主动抛出标准库异常
        throw runtime_error("除数不能为 0");
    }
    return a / b;
}

void TryCatch()
{
    vector<int> v = {1, 2, 3};

    try {
        // v 只有 3 个元素，访问下标 10 会越界
        // at 会做越界检查，抛 out_of_range
        cout << v.at(10) << endl;
    } catch (const out_of_range& e) {
        // 用引用接收异常对象，避免额外拷贝
        cout << "越界：" << e.what() << endl;
    }

    try {
        cout << Divide(10, 0) << endl;
    } catch (const exception& e) {
        // 捕获所有 std::exception 及其子类
        cout << "计算错误：" << e.what() << endl;
    } catch (...) {
        // 兜底，捕获任何类型的异常
        cout << "未知异常" << endl;
    }

    cout << "程序继续运行" << endl;
}

int main()
{
    forLoop();
    whileLoop();

    return 0;
}