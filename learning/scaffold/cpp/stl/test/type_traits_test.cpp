/*
 * type_traits_test.cpp - type_traits 测试
 */

#include "mystl/type_traits.h"
#include <iostream>
#include <type_traits>

int main() {
    std::cout << "=== Type Traits Test ===" << std::endl;

    // 测试 true_type / false_type
    std::cout << "true_type::value = " << mystl::true_type::value << std::endl;
    std::cout << "false_type::value = " << mystl::false_type::value << std::endl;

    // 测试 is_void
    std::cout << "is_void<void>: " << mystl::is_void<void>::value << std::endl;
    std::cout << "is_void<int>: " << mystl::is_void<int>::value << std::endl;

    // 测试 is_integral
    std::cout << "is_integral<int>: " << mystl::is_integral<int>::value << std::endl;
    std::cout << "is_integral<float>: " << mystl::is_integral<float>::value << std::endl;

    // 测试 is_pointer
    std::cout << "is_pointer<int*>: " << mystl::is_pointer<int*>::value << std::endl;
    std::cout << "is_pointer<int>: " << mystl::is_pointer<int>::value << std::endl;

    // 测试 is_same
    static_assert(std::is_same<int, int>::value, "");
    static_assert(!std::is_same<int, float>::value, "");
    std::cout << "is_same: OK" << std::endl;

    // 测试 remove_const
    static_assert(std::is_same<mystl::remove_const_t<const int>, int>::value, "");
    std::cout << "remove_const<const int> -> int: OK" << std::endl;

    // 测试 conditional
    using R1 = mystl::conditional_t<true, int, double>;
    using R2 = mystl::conditional_t<false, int, double>;
    static_assert(std::is_same<R1, int>::value, "");
    static_assert(std::is_same<R2, double>::value, "");
    std::cout << "conditional: OK" << std::endl;

    // 测试 enable_if
    static_assert(!std::is_void<typename mystl::enable_if<true, int>::type>::value, "");
    std::cout << "enable_if: OK" << std::endl;

    // 测试 decay
    static_assert(std::is_same<mystl::decay_t<const int&>, int>::value, "");
    std::cout << "decay: OK" << std::endl;

    // 测试 is_array
    static_assert(mystl::is_array<int[10]>::value, "");
    static_assert(!mystl::is_array<int>::value, "");
    std::cout << "is_array: OK" << std::endl;

    // 测试 remove_reference
    static_assert(std::is_same<mystl::remove_reference_t<int&>, int>::value, "");
    static_assert(std::is_same<mystl::remove_reference_t<int&&>, int>::value, "");
    std::cout << "remove_reference: OK" << std::endl;

    std::cout << "\n=== All Type Traits Tests Passed! ===" << std::endl;
    return 0;
}
