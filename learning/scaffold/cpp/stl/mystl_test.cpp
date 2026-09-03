/*
 * mystl_test.cpp - 全面测试 mystl 24 个组件
 *
 * 测试范围：
 *   基础设施：type_traits / utility / iterator / allocator / functional / memory
 *   序列容器：array / vector / list / forward_list / deque
 *   适配器：stack / queue / priority_queue
 *   有序关联：set / multiset / map / multimap
 *   无序关联：unordered_set / unordered_map / unordered_multiset / unordered_multimap
 *   算法：algorithm / numeric
 */

#include "mystl.h"
#include <cstdio>
#include <cassert>
#include <string>
#include <utility>
#include <cstdlib>

using namespace mystl;

// ============================================================
// 计数器（追踪构造/析构）
// ============================================================

static int g_ctor_count = 0;
static int g_dtor_count = 0;
static int g_copy_ctor = 0;
static int g_move_ctor = 0;
static int g_copy_assign = 0;
static int g_move_assign = 0;

#define RESET_COUNTERS() do { g_ctor_count = g_dtor_count = g_copy_ctor = g_move_ctor = g_copy_assign = g_move_assign = 0; } while (0)
#define SHOW_COUNTERS(label) printf("  [counter %s] ctor=%d dtor=%d copy_c=%d move_c=%d copy_a=%d move_a=%d\n", \
    (label), g_ctor_count, g_dtor_count, g_copy_ctor, g_move_ctor, g_copy_assign, g_move_assign)

// ============================================================
// 1. type_traits
// ============================================================

static void test_type_traits() {
    printf("\n[1] type_traits\n");
    static_assert(is_same<int, int>::value, "is_same");
    static_assert(!is_same<int, long>::value, "is_same");
    static_assert(is_integral<int>::value, "is_integral");
    static_assert(!is_integral<float>::value, "is_integral");
    static_assert(is_arithmetic<int>::value, "is_arithmetic");
    static_assert(is_pointer<int*>::value, "is_pointer");
    static_assert(is_pointer<int**>::value, "is_pointer");
    static_assert(!is_pointer<int>::value, "is_pointer");
    static_assert(is_const<const int>::value, "is_const");
    static_assert(!is_const<int>::value, "is_const");
    static_assert(is_reference<int&>::value, "is_reference");
    static_assert(is_array<int[5]>::value, "is_array");
    static_assert((is_same<remove_reference<int&>::type, int>::value), "remove_reference");
    static_assert((is_same<remove_const<const int>::type, int>::value), "remove_const");
    static_assert((is_same<remove_pointer<int*>::type, int>::value), "remove_pointer");
    static_assert((is_same<add_pointer<int>::type, int*>::value), "add_pointer");
    static_assert((is_same<decay<int[5]>::type, int*>::value), "decay");
    static_assert(is_same<conditional<true, int, long>::type, int>::value, "conditional");
    static_assert(is_same<conditional<false, int, long>::type, long>::value, "conditional");
    printf("  [PASS] 全部静态断言通过\n");
}

// ============================================================
// 2. utility (pair / move / forward / swap)
// ============================================================

static void test_utility() {
    printf("\n[2] utility\n");
    pair<int, std::string> p{42, "hello"};
    assert(p.first == 42);
    assert(p.second == "hello");
    printf("  pair<int,string>(42, hello)\n");

    auto p2 = make_pair(3.14, "pi");
    assert(p2.first == 3.14);
    assert(p2.second == "pi");
    printf("  make_pair(3.14, pi)\n");

    int a = 5, b = 7;
    swap(a, b);
    assert(a == 7 && b == 5);
    printf("  swap: a<->b\n");

    std::string src = "world";
    std::string dst = mystl::move(src);
    assert(dst == "world");
    assert(src.empty());
    printf("  move string: src=empty, dst='%s'\n", dst.c_str());
    printf("  [PASS]\n");
}

// ============================================================
// 3. allocator
// ============================================================

static void test_allocator() {
    printf("\n[3] allocator\n");
    allocator<int> alloc;
    int* p = alloc.allocate(5);
    for (int i = 0; i < 5; ++i) alloc.construct(p + i, i * 10);
    for (int i = 0; i < 5; ++i) assert(p[i] == i * 10);
    printf("  allocate+construct 5 个 int: ");
    for (int i = 0; i < 5; ++i) printf("%d ", p[i]);
    printf("\n");
    for (int i = 0; i < 5; ++i) alloc.destroy(p + i);
    alloc.deallocate(p, 5);
    printf("  destroy+deallocate\n");
    printf("  [PASS]\n");
}

// ============================================================
// 4. functional
// ============================================================

static void test_functional() {
    printf("\n[4] functional\n");
    less<int> lt;
    greater<int> gt;
    assert(lt(1, 2));
    assert(!lt(2, 1));
    assert(gt(2, 1));
    printf("  less / greater: OK\n");
    hash<int> hi;
    assert(hi(42) == 42u);
    printf("  hash<int>(42) == 42: OK\n");
    printf("  [PASS]\n");
}

// ============================================================
// 5. memory (unique_ptr / shared_ptr / weak_ptr)
// ============================================================

static void test_memory() {
    printf("\n[5] memory\n");
    // unique_ptr
    {
        unique_ptr<int> up(new int(42));
        assert(*up == 42);
        printf("  unique_ptr<int>: *up == 42\n");
    }
    // shared_ptr
    {
        shared_ptr<int> sp1(new int(100));
        assert(*sp1 == 100);
        assert(sp1.use_count() == 1);
        shared_ptr<int> sp2 = sp1;
        assert(sp1.use_count() == 2);
        printf("  shared_ptr copy: count=%ld\n", sp1.use_count());
    }
    // weak_ptr
    {
        shared_ptr<int> sp(make_shared<int>(7));
        weak_ptr<int> wp = sp;
        assert(!wp.expired());
        assert(wp.use_count() == 1);
        printf("  weak_ptr: not expired, count=%ld\n", wp.use_count());
    }
    printf("  [PASS]\n");
}

// ============================================================
// 6. array
// ============================================================

static void test_array() {
    printf("\n[6] array\n");
    array<int, 5> a = {1, 2, 3, 4, 5};
    assert(a.size() == 5);
    assert(a[0] == 1);
    assert(a.front() == 1);
    assert(a.back() == 5);
    int sum = 0;
    for (int x : a) sum += x;
    assert(sum == 15);
    printf("  array<int,5>: size=%zu sum=%d\n", a.size(), sum);
    printf("  [PASS]\n");
}

// ============================================================
// 7. vector
// ============================================================

static void test_vector() {
    printf("\n[7] vector\n");
    vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    v.emplace_back(4);
    assert(v.size() == 4);
    assert(v[0] == 1);
    assert(v.back() == 4);

    v.insert(v.begin() + 1, 100);
    assert(v[1] == 100);
    v.pop_back();
    assert(v.size() == 4);
    assert(v.back() == 3);

    printf("  push_back/emplace_back/insert/pop_back\n");
    for (int x : v) printf("    %d\n", x);

    // sort
    vector<int> vs = {5, 2, 8, 1, 9, 3};
    sort(vs.begin(), vs.end());
    assert(vs[0] == 1 && vs[1] == 2 && vs[2] == 3);
    printf("  sort: ");
    for (int x : vs) printf("%d ", x);
    printf("\n");

    printf("  [PASS]\n");
}

// ============================================================
// 8. list
// ============================================================

static void test_list() {
    printf("\n[8] list\n");
    list<int> l = {1, 2, 3, 4, 5};
    assert(l.size() == 5);
    assert(l.front() == 1);
    assert(l.back() == 5);

    l.push_front(0);
    l.push_back(6);
    assert(l.front() == 0 && l.back() == 6);
    assert(l.size() == 7);
    printf("  push_front/push_back size=%zu\n", l.size());

    // sort
    list<int> ls = {5, 2, 8, 1, 9, 3};
    ls.sort();
    int prev = 0;
    for (int x : ls) { assert(x >= prev); prev = x; }
    printf("  list.sort(): OK\n");

    // reverse
    l.reverse();
    assert(l.front() == 6);
    printf("  reverse: front=%d\n", l.front());

    printf("  [PASS]\n");
}

// ============================================================
// 9. forward_list
// ============================================================

static void test_forward_list() {
    printf("\n[9] forward_list\n");
    forward_list<int> fl = {1, 2, 3, 4, 5};
    assert(fl.front() == 1);

    fl.push_front(0);
    assert(fl.front() == 0);

    // insert_after
    auto it = fl.begin();
    ++it;
    fl.insert_after(it, 100);
    printf("  insert_after(2nd pos, 100)\n");

    // reverse
    fl.reverse();
    assert(fl.front() == 5);
    printf("  reverse: front=%d\n", fl.front());

    printf("  [PASS]\n");
}

// ============================================================
// 10. deque
// ============================================================

static void test_deque() {
    printf("\n[10] deque\n");
    deque<int> d = {1, 2, 3, 4, 5};
    assert(d.size() == 5);
    assert(d.front() == 1);
    assert(d.back() == 5);

    d.push_front(0);
    d.push_back(6);
    assert(d.front() == 0 && d.back() == 6);
    assert(d.size() == 7);

    d.pop_front();
    d.pop_back();
    assert(d.size() == 5);
    printf("  deque: push/pop front/back, size=%zu\n", d.size());

    // 随机访问
    printf("  random access: ");
    for (size_t i = 0; i < d.size(); ++i) printf("%d ", d[i]);
    printf("\n");

    printf("  [PASS]\n");
}

// ============================================================
// 11. stack / queue / priority_queue
// ============================================================

static void test_adapters() {
    printf("\n[11] stack/queue/priority_queue\n");
    // stack
    stack<int> s;
    s.push(1); s.push(2); s.push(3);
    assert(s.size() == 3);
    assert(s.top() == 3);
    s.pop();
    assert(s.top() == 2);
    printf("  stack: LIFO (top=%d)\n", s.top());

    // queue
    queue<int> q;
    q.push(1); q.push(2); q.push(3);
    assert(q.front() == 1);
    assert(q.back() == 3);
    q.pop();
    assert(q.front() == 2);
    printf("  queue: FIFO (front=%d back=%d)\n", q.front(), q.back());

    // priority_queue
    priority_queue<int> pq;
    pq.push(3); pq.push(1); pq.push(4); pq.push(1); pq.push(5);
    assert(pq.top() == 5);  // 最大堆
    pq.pop();
    assert(pq.top() == 4);
    pq.pop();
    assert(pq.top() == 3);
    printf("  priority_queue: max-heap (top=%d)\n", pq.top());

    printf("  [PASS]\n");
}

// ============================================================
// 12. set / multiset / map / multimap
// ============================================================

static void test_ordered() {
    printf("\n[12] set/multiset/map/multimap\n");
    // set
    set<int> s = {3, 1, 4, 1, 5, 9, 2, 6};
    assert(s.size() == 7);  // 自动去重
    assert(s.count(5) == 1);
    int prev = 0;
    for (int x : s) { assert(x > prev); prev = x; }
    printf("  set: 自动去重+排序 size=%zu\n", s.size());

    // multiset
    multiset<int> ms = {1, 2, 2, 3, 3, 3, 4};
    assert(ms.count(3) == 3);
    printf("  multiset: count(3)=%zu\n", ms.count(3));

    // map
    map<int, std::string> m;
    m[10] = "alice";
    m[20] = "bob";
    m[30] = "carol";
    assert(m.size() == 3);
    assert(m[20] == "bob");
    printf("  map: m[20]=%s\n", m[20].c_str());

    // multimap
    multimap<int, std::string> mm;
    mm.insert({1, "a"});
    mm.insert({1, "b"});
    mm.insert({1, "c"});
    assert(mm.count(1) == 3);
    printf("  multimap: count(1)=%zu\n", mm.count(1));

    printf("  [PASS]\n");
}

// ============================================================
// 13. unordered_set/multiset/map/multimap
// ============================================================

static void test_unordered() {
    printf("\n[13] unordered set/multiset/map/multimap\n");
    // unordered_set
    unordered_set<int> us = {3, 1, 4, 1, 5, 9, 2, 6};
    assert(us.size() == 7);
    assert(us.count(5) == 1);
    assert(us.find(5) != us.end());
    printf("  unordered_set: 自动去重 size=%zu\n", us.size());

    // unordered_multiset
    unordered_multiset<int> ums = {1, 2, 2, 3, 3, 3, 4};
    assert(ums.count(3) == 3);
    printf("  unordered_multiset: count(3)=%zu\n", ums.count(3));

    // unordered_map
    unordered_map<int, std::string> um;
    um[100] = "x";
    um[200] = "y";
    um[300] = "z";
    assert(um[100] == "x");
    printf("  unordered_map: 100=%s, 200=%s, 300=%s\n",
           um[100].c_str(), um[200].c_str(), um[300].c_str());

    // unordered_multimap
    unordered_multimap<int, std::string> umm;
    umm.insert({1, "a"});
    umm.insert({1, "b"});
    umm.insert({1, "c"});
    assert(umm.count(1) == 3);
    printf("  unordered_multimap: count(1)=%zu\n", umm.count(1));

    printf("  [PASS]\n");
}

// ============================================================
// 14. algorithm
// ============================================================

static void test_algorithm() {
    printf("\n[14] algorithm\n");
    vector<int> v = {5, 2, 8, 1, 9, 3};

    // find
    auto it = find(v.begin(), v.end(), 8);
    assert(it != v.end());
    assert(*it == 8);

    // count
    assert(count(v.begin(), v.end(), 5) == 1);

    // sort
    sort(v.begin(), v.end());
    assert(v[0] == 1);

    // binary_search
    assert(binary_search(v.begin(), v.end(), 8));

    // min_element / max_element
    auto min_it = min_element(v.begin(), v.end());
    auto max_it = max_element(v.begin(), v.end());
    assert(*min_it == 1);
    assert(*max_it == 9);

    // transform
    vector<int> sq(v.size());
    transform(v.begin(), v.end(), sq.begin(), [](int x) { return x * x; });
    assert(sq[0] == 1 && sq[5] == 81);

    // fill
    fill(v.begin(), v.begin() + 3, 99);
    assert(v[0] == 99);

    // reverse
    vector<int> rv = {1, 2, 3, 4, 5};
    reverse(rv.begin(), rv.end());
    assert(rv[0] == 5 && rv[4] == 1);

    printf("  find/count/sort/binary_search/min_element/max_element/transform/fill/reverse\n");

    // lexicographical_compare
    vector<int> a = {1, 2, 3};
    vector<int> b = {1, 2, 4};
    assert(lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()));
    printf("  lexicographical_compare\n");

    printf("  [PASS]\n");
}

// ============================================================
// 15. numeric
// ============================================================

static void test_numeric() {
    printf("\n[15] numeric\n");
    vector<int> v = {1, 2, 3, 4, 5};

    int sum = accumulate(v.begin(), v.end(), 0);
    assert(sum == 15);
    printf("  accumulate: %d\n", sum);

    int prod = inner_product(v.begin(), v.end(), v.begin(), 1);
    assert(prod == 1*1 + 2*2 + 3*3 + 4*4 + 5*5);
    printf("  inner_product: %d\n", prod);

    vector<int> ps(v.size());
    partial_sum(v.begin(), v.end(), ps.begin());
    assert(ps[0] == 1);
    assert(ps[1] == 3);
    assert(ps[4] == 15);
    printf("  partial_sum: ");
    for (int x : ps) printf("%d ", x);
    printf("\n");

    vector<int> ad(v.size());
    adjacent_difference(v.begin(), v.end(), ad.begin());
    assert(ad[0] == 1);
    assert(ad[1] == 1);
    printf("  adjacent_difference: ");
    for (int x : ad) printf("%d ", x);
    printf("\n");

    iota(v.begin(), v.end(), 100);
    assert(v[0] == 100);
    printf("  iota: %d %d %d\n", v[0], v[1], v[2]);

    printf("  [PASS]\n");
}

// ============================================================
// main
// ============================================================

int main() {
    printf("=== mystl 全面测试 ===\n");
    test_type_traits();
    test_utility();
    test_allocator();
    test_functional();
    test_memory();
    test_array();
    test_vector();
    test_list();
    test_forward_list();
    test_deque();
    test_adapters();
    test_ordered();
    test_unordered();
    test_algorithm();
    test_numeric();
    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}