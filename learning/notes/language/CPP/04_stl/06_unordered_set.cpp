#include <iostream>
#include <unordered_set>
using namespace std;

void SetOperation()
{
    unordered_set<int> s;

    // 添加元素
    s.insert(1);
    s.insert(2);
    s.insert(3);
    // 重复添加无效
    s.insert(2);

    // 输出：3（不是 4）
    cout << s.size() << endl;

    // 判断是否包含（C++17 用 count，C++20 可用 contains）
    // 输出：1
    cout << s.count(2) << endl;

    // 删除
    s.erase(2);
    // 输出：0
    cout << s.count(2) << endl;

    // 遍历（顺序不保证）
    for (int x : s) {
        cout << x << " ";
    }
    cout << endl;
}

int main()
{
    SetOperation();
    return 0;
}
