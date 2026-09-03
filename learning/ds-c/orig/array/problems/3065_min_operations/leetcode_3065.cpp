#include <iostream>
#include <vector>

using namespace std;

class Solution {
public:
    int minOperations(vector<int>& nums, int k) {
        int cnt = 0;
        for (int &num : nums) {
            if (num < k) {
                cnt++;
            }
        }

        return cnt;
    }
};

int main()
{
    vector<int> nums = {2,11,10,1,3};
    int k = 10;

    Solution solution;
    int res = solution.minOperations(nums, k);
    cout << "res: " << res << endl;

    return 0;
}