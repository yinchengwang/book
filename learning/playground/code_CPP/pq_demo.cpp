/*
 * @Author: yinchengwang
 * @Date: 2025-02-07 22:51:46
 * @LastEditors: yinchengwang
 * @LastEditTime: 2025-02-08 13:54:34
 * @Description: C++ priority_queue 容器使用方法demo
 * @FilePath: \book\demo_code\CPP\pq_demo.cpp
 */

#include <iostream>
#include <queue>
#include <vector>
#include <functional>

#include <cstdlib>
#include <ctime>

using namespace std;

#define RANDOM_RANGE 50
#define PQ_RANGE 10

bool max_heap_pair_cmp(const pair<int, int> &lhs, const pair<int, int> &rhs)
{
    return lhs.first > rhs.first || (lhs.first == rhs.first && lhs.second < rhs.second);
}

struct MaxHeapPairCmp {
    // 比较函数对象的定义
    bool operator()(const std::pair<int, int>& lhs, const std::pair<int, int>& rhs) const {
        return lhs.first > rhs.first || (lhs.first == rhs.first && lhs.second > rhs.second);
    }
};

int main()
{
    srand(time(NULL));

    // priority_queue容器默认是大顶堆, 默认的函数是less<T>
    priority_queue<int, vector<int>> max_heap;

    for (int i = 0; i < PQ_RANGE; i++) {
        max_heap.push(rand() % RANDOM_RANGE);
    }

    cout << "max_heap priority_queue size: " << max_heap.size() << endl;
    cout << "max_heap priority_queue: ";
    while (!max_heap.empty()) {
        cout << max_heap.top() << " ";
        max_heap.pop();
    }
    cout << endl;

    priority_queue<int, vector<int>, greater<int>> min_heap;
    for (int i = 0; i < PQ_RANGE; i++) {
        min_heap.push(rand() % RANDOM_RANGE);
    }

    cout << "min_heap priority_queue size: " << min_heap.size() << endl;
    cout << "min_heap priority_queue: ";
    while (!min_heap.empty()) {
        cout << min_heap.top() << " ";
        min_heap.pop();
    }
    cout << endl;

    // 定义pair类型的优先队列, 首先根据第一个元素排序, 第一个元素相同时根据第二个排序
    priority_queue<pair<int, int>, vector<pair<int, int>>,
        function<bool(pair<int, int>, pair<int, int>)>> min_pair_heap(
            [](const pair<int, int> &lhs, const pair<int, int> &rhs) {
                return lhs.first < rhs.first || (lhs.first == rhs.first && lhs.second > rhs.second);
            }
        );

    min_pair_heap.push({3, 5});
    min_pair_heap.push({2, 1});
    min_pair_heap.push({3, 4});
    min_pair_heap.push({2, 5});

    while (!min_pair_heap.empty()) {
        // min_pair_heap first: 3 min_pair_heap second: 4
        // min_pair_heap first: 3 min_pair_heap second: 5
        // min_pair_heap first: 2 min_pair_heap second: 1
        // min_pair_heap first: 2 min_pair_heap second: 5
        cout << "min_pair_heap first: " << min_pair_heap.top().first << " ";
        cout << "min_pair_heap second: " << min_pair_heap.top().second << endl;
        min_pair_heap.pop();
    }

    // 第三个模板参数需要的是一个比较函数对象的类型, 不能直接传入函数名
    // priority_queue<pair<int, int>, vector<pair<int, int>>, max_heap_pair_cmp> max_pair_heap;
    priority_queue<pair<int, int>, vector<pair<int, int>>,
        function<bool(pair<int, int>, pair<int, int>)>> max_heap1(max_heap_pair_cmp);

    priority_queue<pair<int, int>, vector<pair<int, int>>, MaxHeapPairCmp> max_heap2;
    max_heap2.push({3, 5});
    max_heap2.push({2, 1});
    max_heap2.push({3, 4});
    max_heap2.push({2, 5});
    while (!max_heap2.empty()) {
        // max_heap2 first: 2 max_heap2 second: 1
        // max_heap2 first: 2 max_heap2 second: 5
        // max_heap2 first: 3 max_heap2 second: 4
        // max_heap2 first: 3 max_heap2 second: 5
        cout << "max_heap2 first: " << max_heap2.top().first << " ";
        cout << "max_heap2 second: " << max_heap2.top().second << endl;
        max_heap2.pop();
    }

    return 0;
}