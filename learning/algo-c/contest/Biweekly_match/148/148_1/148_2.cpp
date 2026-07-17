// 3424. 将数组变相同的最小代价
#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

class Solution {
public:
    long long minCost(vector<int>& arr, vector<int>& brr, long long K) {
        int n = arr.size();

        auto calc = [&]() {
            long long ret = 0;
            for (int i = 0; i < n; i++) ret += abs(brr[i] - arr[i]);
            return ret;
        };
        
        long long ans1 = calc();
        sort(arr.begin(), arr.end());
        sort(brr.begin(), brr.end());
        long long ans2 = calc() + K;

        return min(ans1, ans2);
    }
};


int main()
{
    // TODO: 贪心算法学习
    Solution solution;

    vector<int> arr = {-7,9,5};
    vector<int> brr = {7,-2,-5};
    long long k = 2;
    long long res = solution.minCost(arr, brr, k);
    cout << "res is: " << res << endl;

    return 0;
}
