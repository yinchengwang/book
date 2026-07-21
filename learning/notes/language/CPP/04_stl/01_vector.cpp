#include <vector>
#include <iostream>

using namespace std;

void init()
{
    // 空 vector
    vector<int> v1;

    // 指定大小，默认值为 0
    vector<int> v2(5);       // {0, 0, 0, 0, 0}

    // 指定大小和初始值
    vector<int> v3(5, -1);   // {-1, -1, -1, -1, -1}

    // 初始化列表
    vector<int> v4 = {1, 2, 3, 4, 5};

    // 二维 vector：3 行 4 列，初始值为 0
    vector<vector<int>> grid(3, vector<int>(4, 0));
}

void Common1DOperations()
{
    vector<int> nums = {10, 20, 30};

    // 尾部添加元素
    nums.push_back(40);
    nums.push_back(50);

    // 大小
    // 输出：5
    cout << nums.size() << endl;

    // 判空
    // 输出：0（不为空）
    cout << nums.empty() << endl;

    // 通过下标访问
    // 输出：10
    cout << nums[0] << endl;

    // 最后一个元素
    // 输出：50
    cout << nums.back() << endl;

    // 删除最后一个元素
    nums.pop_back();
    // 输出：40
    cout << nums.back() << endl;

    // 在索引 1 处插入 99
    nums.insert(nums.begin() + 1, 99);

    // 删除索引 2 处的元素
    nums.erase(nums.begin() + 2);

    // 交换两个 vector 的内容
    vector<int> other = {100, 200};
    nums.swap(other);
    // 此时 nums = {100, 200}，other = {10, 99, 30, 40}

    // 用下标遍历
    // 输出：100 200
    for (int i = 0; i < nums.size(); i++) {
        cout << nums[i] << " ";
    }
    cout << endl;

    // 用 range-based for 遍历
    // 输出：10 99 30 40
    for (int x : other) {
        cout << x << " ";
    }
    cout << endl;
}

void Vector2D()
{
    // 创建 3 行 4 列的二维 vector，初始值为 0
    int rows = 3, cols = 4;
    vector<vector<int>> grid(rows, vector<int>(cols, 0));

    // 赋值
    grid[0][0] = 1;
    grid[1][2] = 5;
    grid[2][3] = 9;

    // 行数和列数
    // 输出：3 行 4 列
    cout << grid.size() << " 行 " << grid[0].size() << " 列" << endl;

    // 遍历
    for (int i = 0; i < grid.size(); i++) {
        for (int j = 0; j < grid[0].size(); j++) {
            cout << grid[i][j] << " ";
        }
        cout << endl;
    }
}

int main()
{
    init();
    Common1DOperations();
    Vector2D();

    return 0;
}