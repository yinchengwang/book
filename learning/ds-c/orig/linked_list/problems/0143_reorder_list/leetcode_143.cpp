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
    private:
        ListNode *get_mid_node(ListNode *head) {
            ListNode *slow = head;
            ListNode *fast = head;

            while (fast->next && fast->next->next) {
                slow = slow->next;
                fast = fast->next->next;
            }

            return slow;
        }

        ListNode *reversed_list(ListNode *head) {
            ListNode *curr = head;
            ListNode *pre = nullptr;

            while (curr) {
                ListNode *next = curr->next;
                curr->next = pre;
                pre = curr;
                curr = next;
            }

            return pre;
        }

        void merge_list(ListNode *l1, ListNode *l2) {

            while (l1 && l2) {
                ListNode *tmp1 = l1->next;
                ListNode *tmp2 = l2->next;

                l1->next = l2;
                l1 = tmp1;

                l2->next = tmp1;
                l2 = tmp2;
            }

        }
    public:
        void reorderList(ListNode* head) {
            ListNode *midnode = get_mid_node(head);
            ListNode *l1 = head;
            ListNode *l2 = midnode->next;
            midnode->next = nullptr;

            l2 = reversed_list(l2);

            merge_list(l1, l2);
        }
    };