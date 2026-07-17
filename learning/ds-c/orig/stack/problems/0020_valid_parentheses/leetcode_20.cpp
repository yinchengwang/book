#include <iostream>
#include <stack>

using namespace std;

class Solution {
    private:
        bool no_match(char c, stack<char> &c_stack) {
            switch (c) {
                case ')':
                    return c_stack.top() != '(';
                case '}':
                    return c_stack.top() != '{';
                case ']':
                    return c_stack.top() != '[';
                default:
                    return false;
            }

            return false;
        }
    public:
        bool isValid(string s) {
            stack<char> c_stack;
            for (auto &c: s) {
                if (c == '{' || c == '[' || c == '(') {
                    c_stack.push(c);
                } else if (c_stack.empty() || no_match(c, c_stack)) {
                    return false;
                } else {
                    c_stack.pop();
                }
            }

            return c_stack.empty();
        }
    };