#include "leetcode/leetcode_cpp.h"


// leet_code 118
vector<vector<int>> LeetCode_Solution::generate(int numRows)
{
    vector<vector<int>> res(numRows);
    for (int i = 0; i < numRows; i++) {
        res[i].resize(i + 1);
        res[i][0] = res[i][i] = 1;
        for (int j = 1; j < i; j++) {
            res[i][j] = res[i - 1][j] + res[i - 1][j - 1];
        }
    }

    return res;
}

// leet_code 160
// 1. 循环遍历
ListNode *LeetCode_Solution::getIntersectionNodeLoop(ListNode *headA, ListNode *headB)
{
    if (headA == NULL || headB == NULL) {
        return NULL;
    }

    ListNode *pa = headA;
    ListNode *pb = headB;
    while (pa != pb) {
        pa = pa != NULL ? pa->next : headB;
        pb = pb != NULL ? pb->next : headA;
    }

    return pa;
}

// 2. C++ set容器
ListNode *LeetCode_Solution::getIntersectionNodeSet(ListNode *headA, ListNode *headB)
{
    if (headA == NULL || headB == NULL) {
        return NULL;
    }

    set<ListNode *> listSet;
    ListNode *pa = headA;
    while (pa) {
        listSet.insert(pa);
        pa = pa->next;
    }

    ListNode *pb = headB;
    while (pb) {
        if (listSet.find(pb) != listSet.end()) {
            break;
        }
        pb = pb->next;
    }

    return pb;
}

// 3. 同等的长度再比较
ListNode *LeetCode_Solution::getIntersectionNodeLength(ListNode *headA, ListNode *headB)
{
    if (headA == NULL || headB == NULL) {
        return NULL;
    }

    int lengthA = 0;
    int lengthB = 0;
    ListNode *pa = headA;
    ListNode *pb = headB;

    while (pa) {
        lengthA++;
        pa = pa->next;
    }

    while (pb) {
        lengthB++;
        pb = pb->next;
    }

    int gap = fabs(lengthA - lengthB);
    pa = headA;
    pb = headB;
    if (lengthA > lengthB) {
        while (gap > 0) {
            pa = pa->next;
            gap--;
        }
    } else {
        while (gap > 0) {
            pb = pb->next;
            gap--;
        }
    }

    while (pa && pb) {
        if (pa == pb) {
            break;
        }

        pa = pa->next;
        pb = pb->next;
    }

    return pa;
}