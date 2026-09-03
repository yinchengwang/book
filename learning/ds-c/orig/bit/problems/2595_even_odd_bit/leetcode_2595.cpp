#include <iostream>
#include <vector>

using namespace std;

class Solution {
    public:
        vector<int> evenOddBit1(int n) {
            int even = 0;
            int odd = 0;

            int idx = 0;
            while (n > 0) {
                int mod = n % 2;
                if (idx % 2 == 0 && mod == 1) {
                    even += 1;
                } else if (idx % 2 != 0 && mod == 1) {
                    odd += 1;
                }

                idx++;
                n /= 2;
            }

            return {even, odd};
        }

        vector<int> evenOddBit(int n) {
            vector<int> res = {0, 0};
            int i = 0;
            while (n) {
                // 任何数与1 判断除最后一位的奇偶性
                res[i] += n & 1;
                n >>= 1;
                // 异或1 每次加1 满2进1
                i ^= 1;
            }
            return res;
        }
    };