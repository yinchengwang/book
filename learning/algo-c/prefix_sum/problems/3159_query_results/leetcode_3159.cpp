#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <queue>

using namespace std;

class Solution {
public:
    vector<int> occurrencesOfElement(vector<int>& nums, vector<int>& queries, int x) {
        vector<int> index;
        for (int i = 0; i < nums.size(); i++) {
            if (nums[i] == x) {
                index.push_back(i);
            }
        }

        vector<int> res;
        for (auto q : queries) {
            if (q > index.size()) {
                res.push_back(-1);
            } else {
                res.push_back(index[q - 1]);
            }
        }

        return res;
    }
};

int main()
{
    Solution solution;

    vector<int> nums = {1,3,1,7};
    vector<int> queries = {1,3,2,4};
    int x = 1;

    vector<int> res = solution.occurrencesOfElement(nums, queries, x);
    cout << "res is : ";
    for (auto r : res) {
        cout << r << " ";
    }
    cout << endl;

    nums = {1,2,3};
    queries = {10};
    x = 5;

    res = solution.occurrencesOfElement(nums, queries, x);
    cout << "res is :";
    for (auto r : res) {
        cout << " " << r;
    }
    cout << endl;

    return 0;
}