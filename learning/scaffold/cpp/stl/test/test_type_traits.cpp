/*
 * test_type_traits.cpp - type_traits 全面测试
 *
 * 覆盖：integral_constant / is_same / is_void / is_integral / is_floating_point
 *      / is_arithmetic / is_pointer / is_reference / is_const / is_volatile
 *      / is_array / remove_* / add_* / decay / conditional / enable_if / conjunction
 */

#include "mystl/mystl.h"
#include "mystl_test.h"

using namespace mystl;

namespace {

// 测试类型
struct Foo {};
struct Bar : Foo {};
enum E { E0, E1 };
enum class Ec : int { X, Y };

} // namespace

void test_type_traits() {
    // ============ integral_constant ============
    MYSTL_TEST_BEGIN("integral_constant");
    static_assert(integral_constant<int, 5>::value == 5, "");
    static_assert(true_type::value, "");
    static_assert(!false_type::value, "");
    constexpr integral_constant<int, 42> ic;
    static_assert(ic.value == 42, "");
    static_assert(ic() == 42, "");
    static_assert(bool(ic), "");
    MYSTL_TEST_END();

    // ============ is_same ============
    MYSTL_TEST_BEGIN("is_same");
    static_assert(is_same<int, int>::value, "");
    static_assert(!is_same<int, long>::value, "");
    static_assert(!is_same<const int, int>::value, "");
    static_assert(!is_same<int*, int>::value, "");
    MYSTL_TEST_END();

    // ============ is_void ============
    MYSTL_TEST_BEGIN("is_void");
    static_assert(is_void<void>::value, "");
    static_assert(is_void<const void>::value, "");
    static_assert(is_void<volatile void>::value, "");
    static_assert(!is_void<int>::value, "");
    static_assert(!is_void<int*>::value, "");
    MYSTL_TEST_END();

    // ============ is_null_pointer ============
    MYSTL_TEST_BEGIN("is_null_pointer");
    static_assert(is_null_pointer<decltype(nullptr)>::value, "");
    static_assert(!is_null_pointer<void*>::value, "");
    static_assert(!is_null_pointer<int>::value, "");
    MYSTL_TEST_END();

    // ============ is_integral ============
    MYSTL_TEST_BEGIN("is_integral");
    static_assert(is_integral<bool>::value, "");
    static_assert(is_integral<char>::value, "");
    static_assert(is_integral<signed char>::value, "");
    static_assert(is_integral<unsigned char>::value, "");
    static_assert(is_integral<short>::value, "");
    static_assert(is_integral<unsigned short>::value, "");
    static_assert(is_integral<int>::value, "");
    static_assert(is_integral<unsigned int>::value, "");
    static_assert(is_integral<long>::value, "");
    static_assert(is_integral<unsigned long>::value, "");
    static_assert(is_integral<long long>::value, "");
    static_assert(is_integral<unsigned long long>::value, "");
    static_assert(is_integral<wchar_t>::value, "");
    static_assert(!is_integral<float>::value, "");
    static_assert(!is_integral<double>::value, "");
    static_assert(!is_integral<int*>::value, "");
    static_assert(!is_integral<int&>::value, "");
    // const volatile
    static_assert(is_integral<const int>::value, "");
    static_assert(is_integral<volatile int>::value, "");
    MYSTL_TEST_END();

    // ============ is_floating_point ============
    MYSTL_TEST_BEGIN("is_floating_point");
    static_assert(is_floating_point<float>::value, "");
    static_assert(is_floating_point<double>::value, "");
    static_assert(is_floating_point<long double>::value, "");
    static_assert(!is_floating_point<int>::value, "");
    static_assert(!is_floating_point<float*>::value, "");
    MYSTL_TEST_END();

    // ============ is_arithmetic ============
    MYSTL_TEST_BEGIN("is_arithmetic");
    static_assert(is_arithmetic<int>::value, "");
    static_assert(is_arithmetic<float>::value, "");
    static_assert(!is_arithmetic<void>::value, "");
    static_assert(!is_arithmetic<int*>::value, "");
    MYSTL_TEST_END();

    // ============ is_pointer ============
    MYSTL_TEST_BEGIN("is_pointer");
    static_assert(is_pointer<int*>::value, "");
    static_assert(is_pointer<int**>::value, "");
    static_assert(is_pointer<const int*>::value, "");
    static_assert(!is_pointer<int>::value, "");
    static_assert(!is_pointer<int&>::value, "");
    static_assert(!is_pointer<int[5]>::value, "");
    MYSTL_TEST_END();

    // ============ is_lvalue_reference / is_rvalue_reference / is_reference ============
    MYSTL_TEST_BEGIN("is_reference");
    static_assert(is_reference<int&>::value, "");
    static_assert(!is_reference<int>::value, "");
    static_assert(is_lvalue_reference<int&>::value, "");
    static_assert(!is_rvalue_reference<int&>::value, "");
    MYSTL_TEST_END();

    // ============ is_const / is_volatile ============
    MYSTL_TEST_BEGIN("is_const_volatile");
    static_assert(is_const<const int>::value, "");
    static_assert(!is_const<int>::value, "");
    static_assert(is_volatile<volatile int>::value, "");
    static_assert(!is_volatile<int>::value, "");
    static_assert(is_const<const volatile int>::value, "");
    static_assert(is_volatile<const volatile int>::value, "");
    MYSTL_TEST_END();

    // ============ is_array ============
    MYSTL_TEST_BEGIN("is_array");
    static_assert(is_array<int[5]>::value, "");
    static_assert(is_array<int[]>::value, "");
    static_assert(!is_array<int>::value, "");
    static_assert(!is_array<int*>::value, "");
    static_assert(!is_array<std::nullptr_t>::value, "");
    MYSTL_TEST_END();

    // ============ is_function ============
    MYSTL_TEST_BEGIN("is_function");
    static_assert(is_function<void()>::value, "");
    static_assert(is_function<int(int)>::value, "");
    static_assert(!is_function<int>::value, "");
    MYSTL_TEST_END();

    // ============ remove_cv ============
    MYSTL_TEST_BEGIN("remove_cv");
    static_assert(is_same<typename remove_cv<int>::type, int>::value, "");
    static_assert(is_same<typename remove_cv<const int>::type, int>::value, "");
    static_assert(is_same<typename remove_cv<volatile int>::type, int>::value, "");
    static_assert(is_same<typename remove_cv<const volatile int>::type, int>::value, "");
    // _t 别名
    static_assert(is_same<remove_cv_t<int* const volatile>, int*>::value, "");
    MYSTL_TEST_END();

    // ============ remove_const ============
    MYSTL_TEST_BEGIN("remove_const");
    static_assert(is_same<remove_const_t<const int>, int>::value, "");
    static_assert(is_same<remove_const_t<int>, int>::value, "");
    MYSTL_TEST_END();

    // ============ remove_volatile ============
    MYSTL_TEST_BEGIN("remove_volatile");
    static_assert(is_same<remove_volatile_t<volatile int>, int>::value, "");
    static_assert(is_same<remove_volatile_t<int>, int>::value, "");
    MYSTL_TEST_END();

    // ============ remove_reference ============
    MYSTL_TEST_BEGIN("remove_reference");
    static_assert(is_same<remove_reference_t<int&>, int>::value, "");
    static_assert(is_same<remove_reference_t<int&&>, int>::value, "");
    static_assert(is_same<remove_reference_t<int>, int>::value, "");
    static_assert(is_same<remove_reference_t<const int&>, const int>::value, "");
    MYSTL_TEST_END();

    // ============ remove_pointer ============
    MYSTL_TEST_BEGIN("remove_pointer");
    static_assert(is_same<remove_pointer_t<int*>, int>::value, "");
    static_assert(is_same<remove_pointer_t<int**>, int*>::value, "");
    static_assert(is_same<remove_pointer_t<int>, int>::value, "");
    MYSTL_TEST_END();

    // ============ add_pointer ============
    MYSTL_TEST_BEGIN("add_pointer");
    static_assert(is_same<add_pointer_t<int>, int*>::value, "");
    static_assert(is_same<add_pointer_t<int&>, int*>::value, "");
    static_assert(is_same<add_pointer_t<int&&>, int*>::value, "");
    MYSTL_TEST_END();

    // ============ add_lvalue_reference / add_rvalue_reference ============
    MYSTL_TEST_BEGIN("add_references");
    static_assert(is_same<add_lvalue_reference_t<int>, int&>::value, "");
    static_assert(is_same<add_rvalue_reference_t<int>, int&&>::value, "");
    MYSTL_TEST_END();

    // ============ remove_extent / remove_all_extents ============
    MYSTL_TEST_BEGIN("remove_extent");
    static_assert(is_same<remove_extent_t<int[5]>, int>::value, "");
    static_assert(is_same<remove_extent_t<int[]>, int>::value, "");
    static_assert(is_same<remove_extent_t<int>, int>::value, "");
    // remove_extent<T[N][M]> -> T[M]，注意：remove_extent<T[N][M]> 偏特化匹配 T[N] 中 N=5, T=int[3]
    // 所以 remove_extent_t<int[5][3]> -> int[3]（移除最外层维度）
    static_assert(is_same<remove_extent_t<int[5][3]>, int[3]>::value, "");
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("remove_all_extents");
    static_assert(is_same<remove_all_extents_t<int>, int>::value, "");
    static_assert(is_same<remove_all_extents_t<int[5]>, int>::value, "");
    static_assert(is_same<remove_all_extents_t<int[5][3]>, int>::value, "");
    MYSTL_TEST_END();

    // ============ decay ============
    MYSTL_TEST_BEGIN("decay");
    static_assert(is_same<decay_t<int&>, int>::value, "");
    static_assert(is_same<decay_t<const int&>, int>::value, "");
    static_assert(is_same<decay_t<int[5]>, int*>::value, "");
    static_assert(is_same<decay_t<int(int)>, int(*)(int)>::value, "");
    MYSTL_TEST_END();

    // ============ conditional ============
    MYSTL_TEST_BEGIN("conditional");
    static_assert(is_same<conditional_t<true, int, long>, int>::value, "");
    static_assert(is_same<conditional_t<false, int, long>, long>::value, "");
    MYSTL_TEST_END();

    // ============ enable_if ============
    MYSTL_TEST_BEGIN("enable_if");
    // 编译期即证明 enable_if 工作
    static_assert(is_same<enable_if_t<true, int>, int>::value, "");
    static_assert(is_same<enable_if_t<true, void>, void>::value, "");
    MYSTL_TEST_END();

    // ============ conjunction / disjunction / negation ============
    MYSTL_TEST_BEGIN("conjunction_disjunction_negation");
    static_assert(conjunction<>::value, "");
    static_assert(!conjunction<false_type>::value, "");
    static_assert(conjunction<true_type>::value, "");
    static_assert(conjunction<true_type, true_type>::value, "");
    static_assert(!conjunction<true_type, false_type>::value, "");
    static_assert(disjunction<>::value == false, "");
    static_assert(disjunction<false_type>::value == false, "");
    static_assert(disjunction<false_type, true_type>::value, "");
    static_assert(negation<true_type>::value == false, "");
    static_assert(negation<false_type>::value, "");
    MYSTL_TEST_END();

    // ============ rank / extent ============
    MYSTL_TEST_BEGIN("rank_extent");
    static_assert(rank<int>::value == 0, "");
    static_assert(rank<int[5]>::value == 1, "");
    static_assert(rank<int[5][3]>::value == 2, "");
    static_assert(extent<int[5]>::value == 5, "");
    static_assert(extent<int[5][3], 1>::value == 3, "");
    MYSTL_TEST_END();
}

int main() {
    printf("=== type_traits 全面测试 ===\n");
    test_type_traits();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}