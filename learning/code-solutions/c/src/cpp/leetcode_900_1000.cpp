#include "leetcode/leetcode_cpp.h"

// leet_code 946
bool LeetCode_Solution::validateStackSequences(vector<int>& pushed, vector<int>& popped)
{
    stack<int> st;
    int idx = 0;
    for (auto &p : pushed) {
        st.push(p);
        while (!st.empty() && st.top() == popped[idx]) {
            idx++;
            st.pop();
        }
    }

    return st.empty();
}