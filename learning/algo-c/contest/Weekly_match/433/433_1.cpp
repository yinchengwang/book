// 100548. 变长子数组求和
#include <iostream>
#include <vector>

using namespace std;

#define MAX(a, b) (a) > (b) ? (a) : (b)

class Solution {
public:
    int get_sum(vector<int>& nums, int start, int end) {
        int sum = 0;
        for (int i = start; i <= end; i++) {
            sum += nums[i];
        }

        return sum;
    }

    int subarraySum(vector<int>& nums) {
        int sum = 0;
        for (int i = 0; i < nums.size(); i++) {
            int start = MAX(0, i - nums[i]);
            sum += get_sum(nums, start, i);
        }

        return sum;
    }
};

int main()
{
    Solution solution;

    vector<int> nums = {3,1,1,2};
    int res = solution.subarraySum(nums);
    cout << "res is: " << res << endl;

    nums = {2,3,1};
    res = solution.subarraySum(nums);
    cout << "res is: " << res << endl;

    return 0;
}