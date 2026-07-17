#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <queue>

using namespace std;

class Solution {
public:
    int minOperations(vector<int>& nums, int k) {
        priority_queue<long long, vector<long long>, greater<long long>> pq;
        for (auto num : nums) {
            pq.push(num);
        }

        int cnt = 0;
        while (!pq.empty()) {
            long long num1 = pq.top();
            pq.pop();
            long long mn = num1, mx = num1;
            if (num1 >= k) {
                break;
            }

            if (!pq.empty()) {
                long long num2 = pq.top();
                pq.pop();
                mn = min(num2, num1);
                mx = max(num2, num1);
            }

            long long new_num = (static_cast<long long>(mn) * 2) + static_cast<long long>(mx);
            pq.push(new_num);

            cnt++;
        }

        return cnt;
    }
};

int main()
{
    Solution solution;

    vector<int> nums = {2,11,10,1,3};
    int k = 10;

    int res = solution.minOperations(nums, k);
    cout << "res is : " << res << endl;

    nums = {1,1,2,4,9};
    k = 20;
    res = solution.minOperations(nums, k);
    cout << "res is : " << res << endl;

    return 0;
}