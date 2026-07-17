#include<vector>
#include<iostream>
#include<algorithm>

using namespace std;

class Solution {
public:
    int minimumDifference(vector<int>& nums, int k) {
        sort(nums.begin(), nums.end());
        int n = nums.size();
        int ans = 1e9;
        for (int left = 0, right = k - 1; right < n; left++, right++) {
            ans = min(ans, nums[right] - nums[left]);
        }

        return ans;
    }
};