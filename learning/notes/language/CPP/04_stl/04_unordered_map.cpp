#include <iostream>
#include <unordered_map>
#include <string>
using namespace std;

void MapCount()
{
    unordered_map<char, int> count;
    string s = "hello";
    for (char c : s) {
        count[c]++;
    }
}

void MapInsert()
{
    unordered_map<string, int> map;
    map["apple"] = 3;

    // 此时只有 1 个元素
    // 输出：1
    cout << map.size() << endl;

    // 访问不存在的 key "grape"
    // 这会自动插入 {"grape": 0}！
    cout << map["grape"] << endl;

    // 现在变成了 2 个元素！
    // 输出：2
    cout << map.size() << endl;

    // 所以，判断 key 是否存在一定要用 count，不要用 []
    if (map.count("orange")) {
        cout << map["orange"] << endl;
    } else {
        // 输出：orange not found
        cout << "orange not found" << endl;
    }
}

void MapOperator()
{
    unordered_map<string, int> map;

    // 添加键值对（直接用 [] 赋值）
    map["apple"] = 3;
    map["banana"] = 5;
    map["cherry"] = 2;

    // 通过 key 获取 value
    // 输出：3
    cout << map["apple"] << endl;

    // 判断 key 是否存在（C++17 用 count，C++20 可用 contains）
    // count 返回 1 表示存在，0 表示不存在
    // 输出：1
    cout << map.count("banana") << endl;
    // 输出：0
    cout << map.count("grape") << endl;

    // 删除
    map.erase("cherry");

    // 大小
    // 输出：2
    cout << map.size() << endl;

    // 遍历（顺序不保证）
    for (auto& [key, value] : map) {
        cout << key << " -> " << value << endl;
    }
}

int main()
{
    MapInsert();
    MapCount();
    MapOperator();

    return 0;
}