#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

class Solution {
public:
    long long validSubstringCount(string word1, string word2) {
        vector<int> diff(26, 0);
        for (auto c : word2) {
            diff[c - 'a']--; // hash表存储word2出现过的词
        }

        long long res = 0;
        int cnt = count_if(diff.begin(), diff.end(), [](int c) {return c < 0; });
        auto update = [&] (int c, int add) {
            diff[c] += add;
            if (add == 1 && diff[c] == 0) {
                cnt--; // 窗口增大时, 里面是否包含了word2全部的字母
            } else if (add == -1 && diff[c] == -1) {
                cnt++; // 窗口移动时清理边界的字母
            }
        };

        size_t len = word1.size();
        for (int l = 0, r = 0; l < len; l++) {
            // 找出符合条件的最小窗口
            while (r < len && cnt > 0) {
                update(word1[r] - 'a', 1);
                r++;
            }

            if (cnt == 0) {
                res += len - r + 1;
            }

            // 移动窗口, 清理左边界
            update(word1[l] - 'a', -1);
        }

        return res;
    }
};

int main()
{
    string word1 = "bcca", word2 = "abc";
    Solution solution;
    long long res = solution.validSubstringCount(word1, word2);
    cout << "res is : " << res << endl;

    word1 = "abcabc", word2 = "abc";
    res = solution.validSubstringCount(word1, word2);
    cout << "res is : " << res << endl;

    return 0;
}