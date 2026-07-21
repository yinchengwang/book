#include <iostream>
#include <map>

using namespace std;

void MapOperations()
{
    map<int, string> m;
    m[3] = "three";
    m[1] = "one";
    m[5] = "five";
    m[2] = "two";

    // 遍历是有序的（按 key 排序）
    // 输出：1->one 2->two 3->three 5->five
    for (auto& [key, value] : m) {
        cout << key << "->" << value << " ";
    }
    cout << endl;

    // lower_bound：返回指向 >= 给定 key 的第一个元素的迭代器
    auto it = m.lower_bound(2);
    // 输出：2->two
    cout << it->first << "->" << it->second << endl;

    // upper_bound：返回指向 > 给定 key 的第一个元素的迭代器
    it = m.upper_bound(2);
    // 输出：3->three
    cout << it->first << "->" << it->second << endl;

    // lower_bound(4)：>= 4 的第一个 key 是 5
    it = m.lower_bound(4);
    // 输出：5->five
    cout << it->first << "->" << it->second << endl;

    // 如果找不到，返回 end()
    it = m.lower_bound(100);
    if (it == m.end()) {
        // 输出：not found
        cout << "not found" << endl;
    }
}

int main()
{
    MapOperations();
    return 0;
}