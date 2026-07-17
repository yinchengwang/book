/*
 * vector_pop_back_test.cpp - vector::pop_back 回归测试
 */

#include "mystl/mystl.h"
#include <cstdio>

template <typename T>
class tracking_allocator : public mystl::allocator<T> {
public:
    using base_type = mystl::allocator<T>;
    using pointer = typename base_type::pointer;
    using size_type = typename base_type::size_type;

    template <typename U>
    struct rebind {
        using other = tracking_allocator<U>;
    };

    tracking_allocator() noexcept = default;

    template <typename U>
    tracking_allocator(const tracking_allocator<U>&) noexcept {}

    static void reset() {
        destroy_count = 0;
        last_destroyed = nullptr;
    }

    template <typename U>
    void destroy(U* p) {
        ++destroy_count;
        last_destroyed = p;
        p->~U();
    }

    static int destroy_count;
    static pointer last_destroyed;
};

template <typename T>
int tracking_allocator<T>::destroy_count = 0;

template <typename T>
typename tracking_allocator<T>::pointer tracking_allocator<T>::last_destroyed = nullptr;

bool test_pop_back_destroys_last_element_through_allocator() {
    using allocator_type = tracking_allocator<int>;
    allocator_type::reset();

    mystl::vector<int, allocator_type> values = {10, 20, 30};
    int* expected_destroyed = values.data() + 2;

    values.pop_back();

    return values.size() == 2 && values.back() == 20 && allocator_type::destroy_count == 1 &&
           allocator_type::last_destroyed == expected_destroyed;
}

bool test_pop_back_single_element_boundary() {
    using allocator_type = tracking_allocator<int>;
    allocator_type::reset();

    mystl::vector<int, allocator_type> values = {42};
    int* expected_destroyed = values.data();

    values.pop_back();

    return values.empty() && values.begin() == values.end() && allocator_type::destroy_count == 1 &&
           allocator_type::last_destroyed == expected_destroyed;
}

int main() {
    const bool multiple_elements = test_pop_back_destroys_last_element_through_allocator();
    const bool single_element = test_pop_back_single_element_boundary();

    if (!multiple_elements) {
        std::fprintf(stderr, "多元素 pop_back 未通过分配器销毁原尾元素\n");
    }
    if (!single_element) {
        std::fprintf(stderr, "单元素 pop_back 边界条件未正确销毁唯一元素\n");
    }

    return multiple_elements && single_element ? 0 : 1;
}
