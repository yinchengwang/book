#include <iostream>
#include <stack>

using namespace std;

struct ListNode {
    int val;
    ListNode *next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x) : val(x), next(nullptr) {}
    ListNode(int x, ListNode *next) : val(x), next(next) {}
};

class Solution {
    public:
        // ListNode* addTwoNumbers(ListNode* l1, ListNode* l2) {
        //     stack<int> s1;
        //     stack<int> s2;

        //     while (l1) {
        //         s1.push(l1->val);
        //         l1 = l1->next;
        //     }

        //     while (l2) {
        //         s2.push(l2->val);
        //         l2 = l2->next;
        //     }

        //     int carry = 0;
        //     ListNode *ans = nullptr;
        //     while (!s1.empty() || !s2.empty() || carry != 0) {
        //         int a = s1.empty() ? 0 : s1.top();
        //         int b = s2.empty() ? 0 : s2.top();

        //         if (!s1.empty()) {
        //             s1.pop();
        //         }

        //         if (!s2.empty()) {
        //             s2.pop();
        //         }

        //         int cur = a + b + carry;
        //         carry = cur / 10;
        //         cur %= 10;

        //         ListNode *currnode = new ListNode(cur);
        //         currnode->next = ans;
        //         ans = currnode;
        //     }

        //     return ans;
        // }

        ListNode *reverseList(ListNode *head) {
            ListNode *pre = nullptr;
            ListNode *curr = head;

            while (curr) {
                ListNode *next = curr->next;
                curr->next = pre;
                pre = curr;
                curr = next;
            }

            return pre;
        }

        ListNode* addTwoNumbers(ListNode* l1, ListNode* l2) {
            ListNode *h1 = reverseList(l1);
            ListNode *h2 = reverseList(l2);

            int carry = 0;
            ListNode *ans = nullptr;
            while (h1 || h2 || carry != 0) {
                int a = 0;
                int b = 0;

                if (h1) {
                    a = h1->val;
                    h1 = h1->next;
                }

                if (h2) {
                    b = h2->val;
                    h2 = h2->next;
                }

                int curr = a + b + carry;
                carry = curr / 10;
                curr %= 10;

                ListNode *currnode = new ListNode(curr);
                currnode->next = ans;
                ans = currnode;
            }

            return ans;
        }
    };