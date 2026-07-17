#ifndef LEET_CODE_CPP_H
#define LEET_CODE_CPP_H

#include <vector>
#include <string>
#include <unordered_map>
#include <set>
#include <stack>
#include <algorithm>
#include <cmath>

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

struct ListNode {
    int val;
    ListNode *next;
    ListNode(int x) : val(x), next(NULL) {}
};

class LeetCode_Solution {
public:
    // 1 枚举法
    vector<int> twoSumEnum(vector<int>& nums, int target);
    // 1 hash表
    vector<int> twoSumHash(vector<int>& nums, int target);

    // 7
    int reverse(int x);

    // 8
    int myAtoi(string s);

    // 56
    vector<vector<int>> merge(vector<vector<int>>& intervals);

    // 118
    vector<vector<int>> generate(int numRows);
    
    // 160
    ListNode *getIntersectionNodeLoop(ListNode *headA, ListNode *headB);
    ListNode *getIntersectionNodeSet(ListNode *headA, ListNode *headB);
    ListNode *getIntersectionNodeLength(ListNode *headA, ListNode *headB);

    // 888
    vector<int> fairCandySwap(vector<int>& aliceSizes, vector<int>& bobSizes);

    // 946
    bool validateStackSequences(vector<int>& pushed, vector<int>& popped);

    // 2016
    int maximumDifference(vector<int>& nums);

    // 2131
    int longestPalindrome(vector<string>& words);

    // 2284
    string largestWordCount(vector<string>& messages, vector<string>& senders);

    // 2942
    vector<int> findWordsContaining(vector<string>& words, char x);

    // 3169
    int countDays(int days, vector<vector<int>>& meetings);

    // 3452
    int sumOfGoodNumbers(vector<int>& nums, int k);

    // interview 17.14
    vector<int> smallestK(vector<int>& arr, int k);
};


// leet_code 2034
class StockPrice {
public:
    StockPrice()
    {
        maxTimeStamp = 0;
    }
    
    void update(int timestamp, int price);
    
    int current();
    
    int maximum();
    
    int minimum();

private:
    int maxTimeStamp;
    unordered_map<int, int> timePriceMap;
    multiset<int> prices;
};


#ifdef __cplusplus
}
#endif // extern "C"

#endif // LEET_CODE_CPP_H