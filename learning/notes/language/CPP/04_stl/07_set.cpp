#include <iostream>
#include <set>

using namespace std;

void SetOperation()
{
    set<int> s;
    s.insert(5);
    s.insert(2);
    s.insert(8);
    s.insert(1);

    // 遍历是有序的
    // 输出：1 2 5 8
    for (int x : s) {
        cout << x << " ";
    }
    cout << endl;

    // lower_bound：>= 3 的最小元素
    auto it = s.lower_bound(3);
    // 输出：5
    cout << *it << endl;

    // upper_bound：> 5 的最小元素
    it = s.upper_bound(5);
    // 输出：8
    cout << *it << endl;
}

int main()
{
    SetOperation();
    return 0;
}