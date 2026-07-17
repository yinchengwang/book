#include <iostream>

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
        ListNode* removeNthFromEnd(ListNode* head, int n) {
            ListNode *fast = head;
            ListNode *dummy = new ListNode(0, head);
            ListNode *slow = dummy;

            while (n > 0) {
                fast = fast->next;
                n--;
            }

            while (fast != nullptr) {
                fast = fast->next;
                slow = slow->next;
            }

            slow->next = slow->next->next;
            ListNode *res = dummy->next;
            delete dummy;

            return res;
        }
    };