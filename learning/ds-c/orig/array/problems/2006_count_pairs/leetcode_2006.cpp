#include <iostream>
#include <vector>

using namespace std;

class Solution {
    public:
        int countKDifference(vector<int>& nums, int k) {
            int res = 0;
            int size = nums.size();
            for (int i = 0; i < size; i++) {
                for (int j = i + 1; j < size; j++) {
                    if (abs(nums[i] - nums[j]) == k) {
                        res++;
                    }
                }
            }

            return res;
        }
    };