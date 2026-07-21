#include <iostream>
using namespace std;

int main() {
    // 声明并初始化
    int nums[] = {10, 20, 30, 40, 50};

    // 通过下标访问，下标从 0 开始
    // 输出：10
    cout << nums[0] << endl;
    // 输出：50
    cout << nums[4] << endl;

    // 修改元素
    nums[0] = 99;

    // 数组长度：用 sizeof 计算
    // sizeof(nums) 是整个数组的字节数，sizeof(nums[0]) 是单个元素的字节数
    // 两者相除就是元素个数
    int len = sizeof(nums) / sizeof(nums[0]);
    // 输出：5
    cout << len << endl;

    // 遍历数组
    // 输出：99 20 30 40 50
    for (int i = 0; i < len; i++) {
        cout << nums[i] << " ";
    }
    cout << endl;

    // 指定大小创建，用 {0} 把所有元素初始化为 0
    int arr[5] = {0};
    // 输出：0 0 0 0 0
    for (int i = 0; i < 5; i++) {
        cout << arr[i] << " ";
    }
    cout << endl;

    return 0;
}