/*
 * type_traits.h — 编译期类型查询和变换
 *
 * 对标 <type_traits>，纯手写实现，不依赖 std::
 * 关键设计：所有 _t 别名都定义在使用之前
 * 参考：libstdc++ type_traits
 */

#ifndef MYSTL_TYPE_TRAITS_H
#define MYSTL_TYPE_TRAITS_H

#include <cstddef>  // size_t

MYSTL_NAMESPACE_BEGIN

// ============================================================
// integral_constant — 编译期常量包装
// ============================================================

template <typename T, T v>
struct integral_constant {
    static constexpr T value = v;
    using value_type = T;
    using type = integral_constant;
    constexpr operator value_type() const noexcept { return value; }
    constexpr value_type operator()() const noexcept { return value; }
};

using true_type  = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

// ============================================================
// 类型变换基础（先定义 struct，再开 _t 别名区）
// ============================================================

// remove_const
template <typename T> struct remove_const          { using type = T; };
template <typename T> struct remove_const<const T> { using type = T; };
template <typename T> using remove_const_t = typename remove_const<T>::type;

// remove_volatile
template <typename T> struct remove_volatile             { using type = T; };
template <typename T> struct remove_volatile<volatile T> { using type = T; };
template <typename T> using remove_volatile_t = typename remove_volatile<T>::type;

// remove_reference
template <typename T> struct remove_reference      { using type = T; };
template <typename T> struct remove_reference<T&>   { using type = T; };
template <typename T> struct remove_reference<T&&>  { using type = T; };
template <typename T> using remove_reference_t = typename remove_reference<T>::type;

// remove_cv
template <typename T>
struct remove_cv {
    using type = remove_const_t<remove_volatile_t<T>>;
};
template <typename T> using remove_cv_t = typename remove_cv<T>::type;

// remove_pointer
template <typename T> struct remove_pointer       { using type = T; };
template <typename T> struct remove_pointer<T*>   { using type = T; };
template <typename T> using remove_pointer_t = typename remove_pointer<T>::type;

// add_pointer
template <typename T> struct add_pointer { using type = remove_reference_t<T>*; };
template <typename T> using add_pointer_t = typename add_pointer<T>::type;

// add_lvalue_reference
template <typename T> struct add_lvalue_reference { using type = T&; };
template <typename T> using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;

// add_rvalue_reference
template <typename T> struct add_rvalue_reference { using type = T&&; };
template <typename T> using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

// remove_extent
template <typename T> struct remove_extent       { using type = T; };
template <typename T> struct remove_extent<T[]>  { using type = T; };
template <typename T, size_t N> struct remove_extent<T[N]> { using type = T; };
template <typename T> using remove_extent_t = typename remove_extent<T>::type;

// remove_all_extents
template <typename T> struct remove_all_extents       { using type = T; };
template <typename T> struct remove_all_extents<T[]>  { using type = typename remove_all_extents<T>::type; };
template <typename T, size_t N> struct remove_all_extents<T[N]> { using type = typename remove_all_extents<T>::type; };
template <typename T> using remove_all_extents_t = typename remove_all_extents<T>::type;

// ============================================================
// 类型关系查询（可安全使用 remove_cv_t 等）
// ============================================================

// is_same
template <typename T, typename U> struct is_same : false_type {};
template <typename T> struct is_same<T, T> : true_type {};

// is_void
template <typename T> struct is_void : false_type {};
template <> struct is_void<void> : true_type {};
template <> struct is_void<const void> : true_type {};
template <> struct is_void<volatile void> : true_type {};
template <> struct is_void<const volatile void> : true_type {};

// is_null_pointer
template <typename T> struct is_null_pointer : false_type {};
template <> struct is_null_pointer<decltype(nullptr)> : true_type {};

// is_integral — 所有整数类型特化
template <typename T> struct _is_integral_impl : false_type {};
template <> struct _is_integral_impl<bool>               : true_type {};
template <> struct _is_integral_impl<char>               : true_type {};
template <> struct _is_integral_impl<signed char>        : true_type {};
template <> struct _is_integral_impl<unsigned char>      : true_type {};
template <> struct _is_integral_impl<short>              : true_type {};
template <> struct _is_integral_impl<unsigned short>     : true_type {};
template <> struct _is_integral_impl<int>                : true_type {};
template <> struct _is_integral_impl<unsigned int>       : true_type {};
template <> struct _is_integral_impl<long>               : true_type {};
template <> struct _is_integral_impl<unsigned long>      : true_type {};
template <> struct _is_integral_impl<long long>          : true_type {};
template <> struct _is_integral_impl<unsigned long long> : true_type {};
template <> struct _is_integral_impl<char16_t>           : true_type {};
template <> struct _is_integral_impl<char32_t>           : true_type {};
template <> struct _is_integral_impl<wchar_t>            : true_type {};
template <typename T>
struct is_integral : _is_integral_impl<typename remove_cv<T>::type> {};

// is_floating_point
template <typename T> struct _is_floating_point_impl : false_type {};
template <> struct _is_floating_point_impl<float>       : true_type {};
template <> struct _is_floating_point_impl<double>      : true_type {};
template <> struct _is_floating_point_impl<long double> : true_type {};
template <typename T>
struct is_floating_point : _is_floating_point_impl<typename remove_cv<T>::type> {};

// is_arithmetic — 整数或浮点
template <typename T>
struct is_arithmetic : integral_constant<bool,
    is_integral<T>::value || is_floating_point<T>::value> {};

// is_pointer
template <typename T> struct _is_pointer_impl : false_type {};
template <typename T> struct _is_pointer_impl<T*> : true_type {};
template <typename T> struct is_pointer : _is_pointer_impl<typename remove_cv<T>::type> {};

// is_lvalue_reference
template <typename T> struct is_lvalue_reference : false_type {};
template <typename T> struct is_lvalue_reference<T&> : true_type {};

// is_rvalue_reference
template <typename T> struct is_rvalue_reference : false_type {};
template <typename T> struct is_rvalue_reference<T&&> : true_type {};

// is_reference
template <typename T> struct is_reference : false_type {};
template <typename T> struct is_reference<T&>  : true_type {};
template <typename T> struct is_reference<T&&> : true_type {};

// is_const
template <typename T> struct is_const          : false_type {};
template <typename T> struct is_const<const T> : true_type {};

// is_volatile
template <typename T> struct is_volatile             : false_type {};
template <typename T> struct is_volatile<volatile T> : true_type {};

// is_array
template <typename T> struct is_array             : false_type {};
template <typename T> struct is_array<T[]>        : true_type {};
template <typename T, size_t N> struct is_array<T[N]> : true_type {};

// is_function — 简化版
template <typename T>
struct is_function : integral_constant<bool, !is_reference<T>::value && !is_const<const T>::value> {};

// ============================================================
// 编译期选择
// ============================================================

// conditional（先定义，decay 需要它）
template <bool B, typename T, typename F>
struct conditional { using type = T; };
template <typename T, typename F>
struct conditional<false, T, F> { using type = F; };
template <bool B, typename T, typename F>
using conditional_t = typename conditional<B, T, F>::type;

// decay（依赖 conditional / remove_extent / add_pointer / remove_cv）
template <typename T>
struct decay {
private:
    using _u = typename remove_reference<T>::type;
public:
    using type = typename conditional<
        is_array<_u>::value,
        typename remove_extent<_u>::type*,
        typename conditional<
            is_function<_u>::value,
            typename add_pointer<_u>::type,
            typename remove_cv<_u>::type
        >::type
    >::type;
};
template <typename T> using decay_t = typename decay<T>::type;

// enable_if
template <bool B, typename T = void>
struct enable_if {};
template <typename T>
struct enable_if<true, T> { using type = T; };
template <bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;

// conjunction (C++17)
template <typename...> struct conjunction : true_type {};
template <typename T> struct conjunction<T> : T {};
template <typename T, typename... Rest>
struct conjunction<T, Rest...>
    : conditional_t<bool(T::value), conjunction<Rest...>, T> {};

// disjunction (C++17)
template <typename...> struct disjunction : false_type {};
template <typename T> struct disjunction<T> : T {};
template <typename T, typename... Rest>
struct disjunction<T, Rest...>
    : conditional_t<bool(T::value), T, disjunction<Rest...>> {};

// negation (C++17)
template <typename T>
struct negation : integral_constant<bool, !bool(T::value)> {};

// ============================================================
// 其他编译期工具
// ============================================================

// underlying_type（枚举底层类型，需编译器内建）
template <typename T>
struct underlying_type {
    using type = __underlying_type(T);
};

// rank
template <typename T> struct rank : integral_constant<size_t, 0> {};
template <typename T> struct rank<T[]> : integral_constant<size_t, rank<T>::value + 1> {};
template <typename T, size_t N> struct rank<T[N]> : integral_constant<size_t, rank<T>::value + 1> {};

// extent
template <typename T, unsigned I = 0>
struct extent : integral_constant<size_t, 0> {};
template <typename T> struct extent<T[], 0> : integral_constant<size_t, 0> {};
template <typename T, unsigned I> struct extent<T[], I> : extent<T, I - 1> {};
template <typename T, size_t N> struct extent<T[N], 0> : integral_constant<size_t, N> {};
template <typename T, size_t N, unsigned I> struct extent<T[N], I> : extent<T, I - 1> {};

// is_default_constructible（SFINAE 简化版）
template <typename T, typename = void>
struct is_default_constructible : false_type {};
template <typename T>
struct is_default_constructible<T, decltype(void(T()))> : true_type {};

// is_nothrow_move_constructible（简化版）
template <typename T, typename = void>
struct is_nothrow_move_constructible : false_type {};

MYSTL_NAMESPACE_END

#endif // MYSTL_TYPE_TRAITS_H