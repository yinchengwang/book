/*
 * @Author: yinchengwang
 * @Date: 2025-02-09 13:47:08
 * @LastEditors: yinchengwang
 * @LastEditTime: 2025-02-09 13:47:22
 * @Description: leetcode 436周赛第二题     3447. 将元素分配给有约束条件的组
 * @FilePath: \book\leetcode\a_match\Weekly_match\436\436_2.cpp
 */

#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

class Solution
{
public:
    vector<int> assignElements(vector<int> &groups, vector<int> &elements)
    {
        auto mx = *max_element(groups.begin(), groups.end());
        vector<int> target(mx + 1, -1);
        for (int i = 0; i < elements.size(); i++) {
            int x = elements[i];
            if (x > mx || target[x] >= 0) { // x 及其倍数已被标记
                continue;
            }
            for (int y = x; y <= mx; y += x) { // 枚举 x 的倍数 y
                if (target[y] < 0) {
                    target[y] = i; // 标记 y 可以被 x 整除
                }
            }
        }

        // 回答询问
        for (int &x : groups) {
            x = target[x]; // 原地修改
        }
        return groups;
    }
};

int main()
{
    Solution solution;

    vector<int> groups = {8, 4, 3, 2, 4};
    vector<int> elements = {4, 2};
    vector<int> res = solution.assignElements(groups, elements);

    groups = {2, 3, 5, 7};
    elements = {5, 3, 3};
    res = solution.assignElements(groups, elements);

    return 0;
}