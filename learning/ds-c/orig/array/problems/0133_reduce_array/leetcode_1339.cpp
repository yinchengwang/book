#include<iostream>
#include<vector>
#include<algorithm>

using namespace std;

#define MAX_LEN 100001

class Solution {
public:
    int minSetSize(vector<int>& arr) {
        vector<int> sort_arr(MAX_LEN, 0);
        int len = arr.size();
        for (int i = 0; i < len; i++) {
            sort_arr[arr[i]]++;
        }

        sort(sort_arr.begin(), sort_arr.end(), std::greater<int>());

        int res = 0;
        int tmp_cnt = 0;
        for (int i = 0; i < len; i++) {
            res += 1;
            tmp_cnt += sort_arr[i];
            if (tmp_cnt >= len / 2) {
                break;
            }
        }

        return res;
    }
};