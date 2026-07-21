#include <iostream>
using namespace std;

int main() {
    // 01 读取单个输入
    string name;
    int age;

    // 读取一个字符串和一个整数
    cin >> name >> age;

    // 输出：姓名：Alice
    cout << "姓名：" << name << endl;
    // 输出：年龄：25
    cout << "年龄：" << age << endl;


    // 02 读取一整行输入
    string line;
    // 读取一整行（包括空格）
    getline(cin, line);
    // 输出：How are you?
    cout << line << endl;

    // 03 不断读取整数，直到输入结束
    int x;
    while (cin >> x) {
        cout << x << "\n";
    }

    // 04 不断读取两个整数，直到输入结束
    int a, b;
    while (cin >> a >> b) {
        cout << a + b << "\n";
    }

    return 0;
}