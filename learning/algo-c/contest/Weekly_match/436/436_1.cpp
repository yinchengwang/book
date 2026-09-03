/*
 * @Author: yinchengwang
 * @Date: 2025-02-09 10:41:16
 * @LastEditors: yinchengwang
 * @LastEditTime: 2025-02-09 11:18:09
 * @Description: leetcode 436周赛第一题     3446. 按对角线进行矩阵排序
 * @FilePath: \book\leetcode\a_match\Weekly_match\436\436_1.cpp
 */

#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

class Solution {
    public:
        vector<vector<int>> sortMatrix(vector<vector<int>>& matrix) {
            int n = matrix.size();
            vector<vector<int>> res(n, vector<int>(n));
            // 对主对角线以上的对角线进行排序
            for (int col = 1; col < n; col++) {
                int loop_col = col;
                int row = 0;
                vector<int> diagonal;
                while (row < n && loop_col < n) {
                    diagonal.push_back(matrix[row][loop_col]);
                    loop_col++;
                    row++;
                }

                sort(diagonal.begin(), diagonal.end(), less<int>());

                row = 0;
                loop_col = col;
                while (row < n && loop_col < n) {
                    res[row][loop_col] = diagonal[row];
                    loop_col++;
                    row++;
                }
            }

            // 对主对角线以及下方的对角线进行排序
            for (int row = 0; row < n; row++) {
                int loop_row = row;
                int col = 0;
                vector<int> diagonal;
                while (loop_row < n && col < n) {
                    diagonal.push_back(matrix[loop_row][col]);
                    loop_row++;
                    col++;
                }

                sort(diagonal.begin(), diagonal.end(), greater<int>());

                loop_row = row;
                col = 0;
                int idx = 0;
                while (loop_row < n && col < n) {
                    res[loop_row][col] = diagonal[idx];
                    loop_row++;
                    col++;
                    idx++;
                }
            }

            return res;
        }
    };


int main()
{
    Solution solution;

    vector<vector<int>> grid = {
        {1, 7, 3},
        {9, 8, 2},
        {4, 5, 6}
    };

    vector<vector<int>> res = solution.sortMatrix(grid);

    grid = {
        {0, 1},
        {1, 2},
    };

    res = solution.sortMatrix(grid);

    return 0;
}