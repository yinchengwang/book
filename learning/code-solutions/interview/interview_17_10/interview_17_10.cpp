#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

class Solution {
public:
    int majorityElement(vector<int>& nums) {
        sort(nums.begin(), nums.end());
        int middle = nums.size() / 2;
        int mid_cnt = count(nums.begin(), nums.end(), nums[middle]);

        return mid_cnt > middle ? nums[middle] : -1;
    }
};

int main()
{
    vector<int> nums= {1,2,5,9,5,9,5,5,5};
    Solution solution;
    int res = solution.majorityElement(nums);
    cout << "major is : " << res << endl;

    return 0;
}