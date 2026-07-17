/* 给你2个数组，一个大家的喜好数组，一个是礼物数组
 * 大家喜好是一个队列，礼物是一个堆，当队头不喜欢堆顶的礼物时，队头出队排到队尾
 * 求出最后有多少个礼物是送不出去的
 */

#include <iostream>
#include <queue>
#include <vector>
#include <stack>
#include <algorithm>

using namespace std;

int self_implemented_function(vector<int> &favorites, vector<int> &presents)
{
    queue<int> fQue;
    for (auto &favorite : favorites) {
        fQue.push(favorite);
    }

    stack<int> pStack;
    for (auto iter = presents.rbegin(); iter != presents.rend(); iter++) {
        pStack.push(*iter);
    }

    while (pStack.size() > 0) {
        int loop = 0;
        size_t fSize = fQue.size();
        int fTop = fQue.front();
        int pTop = pStack.top();

        while (fTop != pTop) {
            fQue.pop();
            fQue.push(fTop);
            fTop = fQue.front();
            loop++;

            if (loop >= fSize) {
                break;
            }
        }

        if (loop >= fSize) {
            break;
        }

        fQue.pop();
        pStack.pop();
    }

    return pStack.size();
}

int ai_optimization_function(vector<int> &favorites, vector<int> &presents)
{
    queue<int> fQue;
    for (auto &favorite : favorites) {
        fQue.push(favorite);
    }

    stack<int> pStack;
    for (auto iter = presents.rbegin(); iter != presents.rend(); iter++) {
        pStack.push(*iter);
    }

    vector<int> unsentPresents;
    while (!pStack.empty()) {
        int pTop = pStack.top();
        int loopCount = 0;
        size_t fSize = fQue.size();

        while (!fQue.empty()) {
            int fTop = fQue.front();

            if (fTop == pTop) {
                fQue.pop();
                pStack.pop();
                break;
            } else {
                fQue.pop();
                fQue.push(fTop);
                loopCount++;
            }

            if (loopCount >= fSize) {
                // 应该用这种方法
                // 如果存在 favorite [1 1 1 1] presents[2 1 1 1]
                // 自实现的函数则会返回4，实际未分配的礼物应该只有2这1个
                unsentPresents.push_back(pTop);
                pStack.pop();
                break;
            }
        }
    }

    return unsentPresents.size();
}

int main()
{
    vector<int> favorites = {1, 2, 2, 1, 2};
    vector<int> presents = {2, 2, 1, 2, 2};

    int my_result = self_implemented_function(favorites, presents);  // OK
    int results = ai_optimization_function(favorites, presents);  // OK

    cout << "Unsent presents: " << my_result << " " << results << endl;

    favorites = {2, 2, 2, 2, 2};
    presents = {1, 2, 1, 2, 2};

    my_result = self_implemented_function(favorites, presents);  // ERROR
    results = ai_optimization_function(favorites, presents);  // OK

    cout << "Unsent presents: " << my_result << " " << results << endl;

    return 0;
}