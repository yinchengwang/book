#include "leetcode/leetcode_cpp.h"

// leet_code 2016
int LeetCode_Solution::maximumDifference(vector<int>& nums)
{
    if (nums.empty()) {
        return -1;
    }

    int res = -1;
    int min_num = nums[0];

    for (size_t i = 1; i < nums.size(); i++) {
        if (nums[i] > min_num) {
            res = max(res, nums[i] - min_num);
        } else {
            min_num = nums[i];
        }
    }

    return res;
}

// leet_code 2034
void StockPrice::update(int timestamp, int price)
{
    maxTimeStamp = max(timestamp, maxTimeStamp);
    int prePrice = timePriceMap.count(timestamp) ? timePriceMap[timestamp] : 0;
    timePriceMap[timestamp] = price;
    if (prePrice > 0) {
        auto it = prices.find(prePrice);
        if (it != prices.end()) {
            prices.erase(it);
        }
    }
    prices.emplace(price);
}

int StockPrice::current()
{
    return timePriceMap[maxTimeStamp];
}

int StockPrice::maximum()
{
    return *prices.rbegin();
}

int StockPrice::minimum()
{
    return *prices.begin();
}
