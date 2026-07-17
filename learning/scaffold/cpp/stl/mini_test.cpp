#include "mystl.h"
#include <cstdio>
#include <cassert>

using namespace mystl;

int main() {
    printf("=== 简易测试 ===\n");

    // Type traits
    printf("[1] type_traits\n");
    static_assert(is_integral<int>::value, "");
    static_assert(!is_integral<float>::value, "");
    printf("  OK\n");

    // array
    printf("[2] array\n");
    array<int, 3> a = {1, 2, 3};
    printf("  size=%zu sum=%d\n", a.size(), a[0] + a[1] + a[2]);

    // vector
    printf("[3] vector\n");
    vector<int> v;
    for (int i = 0; i < 5; ++i) v.push_back(i);
    printf("  size=%zu v[2]=%d\n", v.size(), v[2]);

    // list
    printf("[4] list\n");
    {
        list<int> l;
        l.push_back(1);
        l.push_back(2);
        l.push_back(3);
        printf("  list size=%zu\n", l.size());
    }

    // forward_list
    printf("[5] forward_list\n");
    {
        forward_list<int> fl;
        fl.push_front(1);
        fl.push_front(2);
        fl.push_front(3);
        printf("  fl front=%d\n", fl.front());
    }

    // deque
    printf("[6] deque\n");
    {
        deque<int> d;
        d.push_back(1);
        d.push_back(2);
        d.push_back(3);
        printf("  d.size=%zu d[1]=%d\n", d.size(), d[1]);
    }

    // stack
    printf("[7] stack\n");
    {
        stack<int> s;
        s.push(1);
        s.push(2);
        printf("  top=%d\n", s.top());
    }

    // queue
    printf("[8] queue\n");
    {
        queue<int> q;
        q.push(1);
        q.push(2);
        printf("  front=%d back=%d\n", q.front(), q.back());
    }

    // priority_queue
    printf("[9] priority_queue\n");
    {
        priority_queue<int> pq;
        pq.push(3);
        pq.push(1);
        pq.push(4);
        printf("  top=%d\n", pq.top());
    }

    // set
    printf("[10] set\n");
    {
        set<int> s;
        s.insert(3);
        s.insert(1);
        s.insert(5);
        printf("  set size=%zu count(3)=%zu\n", s.size(), s.count(3));
    }

    // multiset
    printf("[11] multiset\n");
    {
        multiset<int> ms;
        ms.insert(3);
        ms.insert(3);
        ms.insert(3);
        printf("  multiset count(3)=%zu\n", ms.count(3));
    }

    // map
    printf("[12] map\n");
    {
        map<int, int> m;
        m[1] = 10;
        m[2] = 20;
        m[3] = 30;
        printf("  map size=%zu m[2]=%d\n", m.size(), (m.find(2) != m.end() ? m[2] : -1));
    }

    // multimap
    printf("[13] multimap\n");
    {
        multimap<int, int> mm;
        mm.insert({1, 100});
        mm.insert({1, 200});
        mm.insert({1, 300});
        printf("  multimap count(1)=%zu\n", mm.count(1));
    }

    // unordered_set
    printf("[14] unordered_set\n");
    {
        unordered_set<int> us;
        us.insert(3);
        us.insert(1);
        us.insert(5);
        printf("  us size=%zu count(3)=%zu\n", us.size(), us.count(3));
    }

    // unordered_map
    printf("[15] unordered_map\n");
    {
        unordered_map<int, int> um;
        um[1] = 10;
        um[2] = 20;
        um[3] = 30;
        printf("  um size=%zu um[2]=%d\n", um.size(), um[2]);
    }

    printf("\n=== 简易测试完成 ===\n");
    return 0;
}