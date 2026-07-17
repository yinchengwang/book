#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>

using namespace std;

class Solution {
public:
    int waysToSplitArray(vector<int>& nums) {
        // long long int sum = 0;
        // for (auto num : nums) {
        //     sum += num;
        // }
        // cout << "sum: " << sum << endl;
        long long sum = accumulate(nums.begin(), nums.end(), 0LL);

        long long int cur_sum = 0;
        int res = 0;
        size_t size = nums.size();
        for (int i = 0; i < size - 1; i++) {
            cur_sum += nums[i];
            sum -= nums[i];
            if (cur_sum >= sum) {
                res++;
            }
        }

        return res;
    }
};

int main()
{
    vector<int> nums= {10,4,-8,7};
    Solution solution;
    int res = solution.waysToSplitArray(nums);
    cout << "res is : " << res << endl;

    return 0;
}