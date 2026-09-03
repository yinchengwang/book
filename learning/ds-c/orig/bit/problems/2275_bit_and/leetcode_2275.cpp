#include <iostream>
#include <vector>

using namespace std;

class Solution {
public:
    int largestCombination(vector<int>& candidates) {
        auto max_len = [&](int k) -> int {
            int res = 0;
            for (int num : candidates) {
                if (num & (1 << k)) {
                    res++;
                }
            }

            return res;
        };

        int res = 0;
        for (int i = 0; i < 24; i++) {
            res = max(res, max_len(i));
        }

        return res;
    }
};

int main()
{
    vector<int> candidates = {16,17,71,62,12,24,14};

    Solution sl;
    int res = sl.largestCombination(candidates);

    cout << "res: " << res << endl;

    return 0;
}