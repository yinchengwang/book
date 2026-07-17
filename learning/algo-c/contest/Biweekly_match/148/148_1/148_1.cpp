// 循环数组中相邻元素的最大差值
#include <iostream>
#include <math.h>
#include <vector>

using namespace std;

#define MAX(a, b) (a) > (b) ? (a) : (b)

class Solution {
public:
    int maxAdjacentDistance(vector<int>& nums) {
        size_t len = nums.size();
        int res = fabs(nums[len - 1] - nums[0]);
        for (int i = 0; i < len - 1; i++) {
            res = MAX(res, fabs(nums[i] - nums[i + 1]));
        }

        return res;
    }
};

int main()
{
    Solution solution;

    vector<int> nums = {1, 2, 4};
    int res = solution.maxAdjacentDistance(nums);
    cout << "res is: " << res << endl;

    nums = {-5, 10, -5};
    res = solution.maxAdjacentDistance(nums);
    cout << "res is: " << res << endl;

    return 0;
}