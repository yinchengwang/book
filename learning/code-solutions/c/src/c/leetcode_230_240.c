#include "leetcode/leetcode.h"

// leetcode 231
bool isPowerOfTwo(int n)
{
    if (n == 0) {
        return false;
    }

    if (n == 1) {
        return true;
    }

    long long res = 1;
    while (true) {
        res *= 2;
        if (res == n) {
            return true;
        } else if (res > n) {
            return false;
        }
    }

    return false;
}