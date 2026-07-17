/**
 * @file main.cpp
 * @brief 表达式模板演示
 *
 * 演示：惰性求值、矩阵运算、表达式树
 * 编译：g++ -std=c++17 -o main main.cpp
 */

#include <iostream>
#include <vector>
#include <array>
#include <type_traits>

using std::cout;
using std::endl;

// ============================================================================
// 1. 表达式模板基础：惰性求值
// ============================================================================
template<typename T>
class Scalar {
public:
    explicit Scalar(T value) : value_(value) {}

    // 惰性求值：只存储表达式，不计算
    template<typename Index>
    T operator[](Index) const { return value_; }

    size_t size() const { return 1; }

private:
    T value_;
};

// ============================================================================
// 2. 向量表达式模板
// ============================================================================
template<typename E>
class VecExpression {
public:
    // 自动类型转换
    operator E&() { return static_cast<E&>(*this); }
    operator const E&() const { return static_cast<const E&>(*this); }

    // 通用下标访问
    template<typename Index>
    auto operator[](Index i) const { return static_cast<const E&>(*this)[i]; }

    size_t size() const { return static_cast<const E&>(*this).size(); }
};

// 具体向量类
template<typename T, size_t N>
class Vec : public VecExpression<Vec<T, N>> {
public:
    Vec() : data_({}) {}
    Vec(std::initializer_list<T> init) {
        size_t i = 0;
        for (auto v : init) data_[i++] = v;
    }

    template<typename E>
    Vec(const VecExpression<E>& expr) {
        for (size_t i = 0; i < N; ++i)
            data_[i] = expr[i];
    }

    T operator[](size_t i) const { return data_[i]; }
    T& operator[](size_t i) { return data_[i]; }
    size_t size() const { return N; }

    // 赋值运算符接受任何表达式
    template<typename E>
    Vec& operator=(const VecExpression<E>& expr) {
        for (size_t i = 0; i < N; ++i)
            data_[i] = expr[i];
        return *this;
    }

private:
    std::array<T, N> data_;
};

// 向量加法表达式
template<typename E1, typename E2>
class VecAdd : public VecExpression<VecAdd<E1, E2>> {
public:
    VecAdd(const E1& lhs, const E2& rhs) : lhs_(lhs), rhs_(rhs) {}

    auto operator[](size_t i) const { return lhs_[i] + rhs_[i]; }
    size_t size() const { return lhs_.size(); }

private:
    const E1& lhs_;
    const E2& rhs_;
};

// 向量数乘表达式
template<typename E, typename T>
class VecScale : public VecExpression<VecScale<E, T>> {
public:
    VecScale(const E& vec, T scalar) : vec_(vec), scalar_(scalar) {}

    auto operator[](size_t i) const { return vec_[i] * scalar_; }
    size_t size() const { return vec_.size(); }

private:
    const E& vec_;
    T scalar_;
};

// 加法运算符
template<typename E1, typename E2>
VecAdd<E1, E2> operator+(const VecExpression<E1>& lhs, const VecExpression<E2>& rhs) {
    return VecAdd<E1, E2>(static_cast<const E1&>(lhs), static_cast<const E2&>(rhs));
}

// 数乘运算符
template<typename E, typename T>
VecScale<E, T> operator*(const VecExpression<E>& vec, T scalar) {
    return VecScale<E, T>(static_cast<const E&>(vec), scalar);
}

template<typename E, typename T>
VecScale<E, T> operator*(T scalar, const VecExpression<E>& vec) {
    return vec * scalar;
}

// ============================================================================
// 3. 表达式模板的性能优势
// ============================================================================
void demonstratePerformance() {
    cout << "=== 表达式模板性能优势 ===" << endl;
    cout << "传统方式: v3 = v1 + v2" << endl;
    cout << "  1. 创建临时对象 v1+v2" << endl;
    cout << "  2. 拷贝到 v3" << endl;
    cout << "  3. 销毁临时对象" << endl;
    cout << endl;
    cout << "表达式模板: v3 = v1 + v2" << endl;
    cout << "  1. 直接计算 v3[i] = v1[i] + v2[i]" << endl;
    cout << "  2. 无临时对象，无额外拷贝" << endl;
}

// ============================================================================
// 4. 简单矩阵表达式模板
// ============================================================================
template<typename E>
class MatrixExpression {
public:
    operator const E&() const { return static_cast<const E&>(*this); }

    template<typename I, typename J>
    auto operator()(I i, J j) const { return static_cast<const E&>(*this)(i, j); }
};

template<size_t Rows, size_t Cols>
class Matrix : public MatrixExpression<Matrix<Rows, Cols>> {
public:
    Matrix() : data_({}) {}

    template<typename E>
    Matrix(const MatrixExpression<E>& expr) {
        for (size_t i = 0; i < Rows; ++i)
            for (size_t j = 0; j < Cols; ++j)
                data_[i][j] = expr(i, j);
    }

    template<typename E>
    Matrix& operator=(const MatrixExpression<E>& expr) {
        for (size_t i = 0; i < Rows; ++i)
            for (size_t j = 0; j < Cols; ++j)
                data_[i][j] = expr(i, j);
        return *this;
    }

    auto operator()(size_t i, size_t j) const { return data_[i][j]; }
    auto& operator()(size_t i, size_t j) { return data_[i][j]; }

private:
    std::array<std::array<double, Cols>, Rows> data_;
};

template<typename E1, typename E2, size_t R, size_t C>
class MatrixAdd : public MatrixExpression<MatrixAdd<E1, E2, R, C>> {
public:
    MatrixAdd(const E1& a, const E2& b) : a_(a), b_(b) {}

    auto operator()(size_t i, size_t j) const { return a_(i, j) + b_(i, j); }

private:
    const E1& a_;
    const E2& b_;
};

// 简化版：直接用具体类型
template<size_t R, size_t C>
Matrix<R, C> operator+(const Matrix<R, C>& a, const Matrix<R, C>& b) {
    Matrix<R, C> result;
    for (size_t i = 0; i < R; ++i)
        for (size_t j = 0; j < C; ++j)
            result(i, j) = a(i, j) + b(i, j);
    return result;
}

// ============================================================================
// 客户端代码
// ============================================================================
int main() {
    cout << "=== 表达式模板演示 ===" << endl << endl;

    // 1. 惰性求值
    cout << "1. Lazy Evaluation:" << endl;
    Scalar<int> s(5);
    cout << "Scalar[0] = " << s[0] << endl;
    cout << "Scalar[100] = " << s[100] << endl;

    // 2. 向量表达式
    cout << "\n2. Vector Expressions:" << endl;
    Vec<int, 3> v1 = {1, 2, 3};
    Vec<int, 3> v2 = {4, 5, 6};

    // 表达式模板：惰性求值，无临时对象
    auto result = v1 + v2;
    cout << "v1 + v2 = ";
    for (size_t i = 0; i < 3; ++i) cout << result[i] << " ";
    cout << endl;

    // 链式表达式
    auto chained = v1 + v2 * 2;
    cout << "v1 + v2 * 2 = ";
    for (size_t i = 0; i < 3; ++i) cout << chained[i] << " ";
    cout << endl;

    // 直接赋值给向量
    Vec<int, 3> v3;
    v3 = v1 + v2 + Vec<int, 3>{1, 1, 1};
    cout << "v3 = v1 + v2 + {1,1,1} = ";
    for (size_t i = 0; i < 3; ++i) cout << v3[i] << " ";
    cout << endl;

    // 3. 矩阵表达式
    cout << "\n3. Matrix Expressions:" << endl;
    Matrix<2, 2> m1;
    m1(0, 0) = 1; m1(0, 1) = 2;
    m1(1, 0) = 3; m1(1, 1) = 4;

    Matrix<2, 2> m2;
    m2(0, 0) = 5; m2(0, 1) = 6;
    m2(1, 0) = 7; m2(1, 1) = 8;

    auto m3 = m1 + m2;
    cout << "m1 + m2 =" << endl;
    for (size_t i = 0; i < 2; ++i) {
        cout << "  ";
        for (size_t j = 0; j < 2; ++j)
            cout << m3(i, j) << " ";
        cout << endl;
    }

    // 4. 性能优势
    cout << "\n4. Performance:" << endl;
    demonstratePerformance();

    return 0;
}
